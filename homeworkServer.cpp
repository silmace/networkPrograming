#include <Winsock2.h>
#include <Windows.h>
#include <iostream>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define BUF_SIZE 1024
#define PORT 9999
#define READ 1
#define WRITE 2

// Socket信息结构体
typedef struct
{
    SOCKET hClntSock;
    SOCKADDR_IN clntAddr;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

// 缓冲区信息结构体
typedef struct
{
    OVERLAPPED overlapped;
    WSABUF wsaBuf;
    char buffer[BUF_SIZE];
    int rwMode; // READ or WRITE
} PER_IO_DATA, *LPPER_IO_DATA;

// Socket集合信息结构体
typedef struct
{
    vector<LPPER_HANDLE_DATA> lpSockInfoSet; // 使用vector代替数组
    LONG numofsock;                          // 使用LONG进行原子操作
} SOCK_DATA_SET, *LPSOCK_DATA_SET;

SOCK_DATA_SET sockSet;

BOOL AddSock(LPPER_HANDLE_DATA lpSockInfo);                                                       // 添加Socket到集合中
BOOL DeleteSock(LPPER_HANDLE_DATA lpSockInfo);                                                    // 从集合中删除Socket
BOOL SendToAll(LPPER_IO_DATA lpIOInfo, DWORD bytesTrans);                                         // 向所有Socket发送数据
void ErrorHandling(const char *message);                                                          // 错误处理函数
DWORD WINAPI IOHandlingThread(LPVOID CompletionPortIO);                                           // IO处理线程
void CreateNewWriteInfoFromReadInfo(LPPER_IO_DATA lpSrc, LPPER_IO_DATA *lpDes, DWORD bytesTrans); // 从读取信息创建新的写入信息

int main(int argc, char *argv[])
{
    memset(&sockSet, 0, sizeof(SOCK_DATA_SET)); // 初始化Socket集合
    WSADATA wsaData;                            // wsaData结构体用于存储被WSAStartup函数调用后返回的Windows Sockets数据
    HANDLE hComPort;                            // 完成端口句柄
    LPPER_HANDLE_DATA lpSockInfo;               // Socket信息结构体
    LPPER_IO_DATA lpIOInfo;                     // 缓冲区信息结构体
    SYSTEM_INFO sysInfo;                        // 系统信息结构体
    SOCKET hListenSock, hClientSock;            // 监听Socket和客户端Socket
    SOCKADDR_IN listenAddr, clientAddr;         // 监听地址和客户端地址
    int addrSize;                               // 地址大小
    DWORD recvBytes, flags = 0;                 // 接收字节数和标志

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup() 错误");

    // 创建完成端口
    hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    // 创建IO处理线程
    GetSystemInfo(&sysInfo);
    for (int i = 0; i < sysInfo.dwNumberOfProcessors * 2; i++)
        CreateThread(NULL, 0, IOHandlingThread, (LPVOID)hComPort, 0, NULL);

    // 创建监听套接字
    hListenSock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    memset(&listenAddr, 0, sizeof(listenAddr));
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    listenAddr.sin_port = htons(PORT);

    // 绑定和监听套接字
    if (bind(hListenSock, (SOCKADDR *)&listenAddr, sizeof(listenAddr)) != 0)
        ErrorHandling("bind() 错误");
    if (listen(hListenSock, 5) != 0)
        ErrorHandling("listen() 错误");

    cout << "服务器启动在端口 " << PORT << " 等待连接...\n";

    // 接收客户端连接请求
    while (1)
    {
        addrSize = sizeof(clientAddr);

        // 接收客户端连接请求
        hClientSock = accept(hListenSock, (SOCKADDR *)&clientAddr, &addrSize);
        if (hClientSock == INVALID_SOCKET)
            ErrorHandling("accept() 错误");

        // 创建Socket信息结构体
        lpSockInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
        lpSockInfo->hClntSock = hClientSock;
        lpSockInfo->clntAddr = clientAddr;

        // 将Socket绑定到完成端口
        CreateIoCompletionPort((HANDLE)hClientSock, hComPort, (ULONG_PTR)lpSockInfo, 0);

        // 创建缓冲区信息结构体
        lpIOInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
        memset(&(lpIOInfo->overlapped), 0, sizeof(PER_IO_DATA));
        lpIOInfo->wsaBuf.len = BUF_SIZE;
        lpIOInfo->wsaBuf.buf = lpIOInfo->buffer;
        lpIOInfo->rwMode = READ;

        // 接收客户端消息
        WSARecv(lpSockInfo->hClntSock, &(lpIOInfo->wsaBuf), 1, &recvBytes, &flags, &(lpIOInfo->overlapped), NULL);

        // 将Socket添加到集合中
        AddSock(lpSockInfo);
    }

    closesocket(hListenSock);
    WSACleanup();

    return 0;
}

// 添加Socket到集合中
BOOL AddSock(LPPER_HANDLE_DATA lpSockInfo)
{
    // 使用原子操作保证线程安全
    LONG numofsock = InterlockedIncrement(&sockSet.numofsock);
    if (numofsock <= BUF_SIZE)
    {
        sockSet.lpSockInfoSet.push_back(lpSockInfo);
        return TRUE;
    }
    else
    {
        // 处理数组已满的情况
        return FALSE;
    }
}

// 从集合中删除Socket
BOOL DeleteSock(LPPER_HANDLE_DATA lpSockInfo)
{
    LONG numofsock = sockSet.numofsock;
    vector<LPPER_HANDLE_DATA> &sockInfoArr = sockSet.lpSockInfoSet;

    for (int i = 0; i < numofsock; i++)
    {
        if (lpSockInfo == sockInfoArr[i])
        {
            memmove(&sockInfoArr[i], &sockInfoArr[i + 1], (numofsock - i - 1) * sizeof(LPPER_HANDLE_DATA));
            InterlockedDecrement(&sockSet.numofsock);
            free(lpSockInfo);
            return TRUE;
        }
    }

    return FALSE; // Socket未找到
}

// 错误处理函数
void ErrorHandling(const char *message)
{
    cerr << message << "，错误代码 " << WSAGetLastError() << endl;
    exit(1);
}

// IO处理线程
DWORD WINAPI IOHandlingThread(LPVOID CompletionPortIO)
{
    HANDLE hComPort = (HANDLE)CompletionPortIO; // 完成端口句柄
    SOCKET sock;                                // 客户端Socket
    DWORD bytesTrans;                           // 接收字节数
    LPPER_IO_DATA lpIOInfo;                     // 缓冲区信息结构体
    LPPER_HANDLE_DATA lpSockInfo;               // Socket信息结构体
    DWORD flags = 0;

    while (1)
    {
        if (!GetQueuedCompletionStatus(hComPort, &bytesTrans, (PULONG_PTR)&lpSockInfo, (LPOVERLAPPED *)&lpIOInfo, INFINITE))
        {
            ErrorHandling("GetQueuedCompletionStatus() 错误");
            return 0;
        }
        // 获取Socket
        sock = lpSockInfo->hClntSock;
        // 处理接收到的消息
        if (lpIOInfo->rwMode == READ)
        {
            if (bytesTrans == 0) // EOF
            {
                DeleteSock(lpSockInfo);
                free(lpIOInfo);
                continue;
            }

            // 打印接收到的消息
            cout << "接收到的消息: " << lpIOInfo->buffer << endl;

            SendToAll(lpIOInfo, bytesTrans);
            lpIOInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
            memset(&(lpIOInfo->overlapped), 0, sizeof(PER_IO_DATA));
            lpIOInfo->wsaBuf.len = BUF_SIZE;
            lpIOInfo->wsaBuf.buf = lpIOInfo->buffer;
            lpIOInfo->rwMode = READ;
            WSARecv(sock, &(lpIOInfo->wsaBuf), 1, NULL, &flags, &(lpIOInfo->overlapped), NULL);
        }
        else
        {
            free(lpIOInfo);
        }
    }

    return 0;
}

// 向所有Socket发送数据
BOOL SendToAll(LPPER_IO_DATA lpIOInfo, DWORD bytesTrans)
{
    // 打印发送的消息
    cout << "转发消息: " << lpIOInfo->buffer << endl;
    // 向所有Socket发送数据
    for (int i = 0; i < sockSet.numofsock; i++)
    {
        SOCKET sock = sockSet.lpSockInfoSet[i]->hClntSock;
        LPPER_IO_DATA lpWriteInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
        memcpy(lpWriteInfo, lpIOInfo, sizeof(PER_IO_DATA));
        lpWriteInfo->wsaBuf.buf = lpWriteInfo->buffer;
        lpWriteInfo->wsaBuf.len = bytesTrans;
        lpWriteInfo->rwMode = WRITE;
        WSASend(sock, &(lpWriteInfo->wsaBuf), 1, NULL, 0, &(lpWriteInfo->overlapped), NULL);
    }
    return TRUE;
}
