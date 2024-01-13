#include <iostream>
#include <thread>
#include <cstring>
#include <WinSock2.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")

#define BUF_SIZE 100          // 缓冲区大小
#define NAME_SIZE 20          // 用户名长度
#define PORT 9999             // 服务器端口号
#define SERVER_IP "127.0.0.1" // 服务器 ip 地址

using namespace std;

unsigned WINAPI SendMsg(void *arg); // 发送消息线程函数
unsigned WINAPI RecvMsg(void *arg); // 接收消息线程函数
void ErrorHandling(char *msg);      // 错误处理函数

char name[NAME_SIZE] = "[DEFAULT]"; // 用户名
char msg[BUF_SIZE];                 // 消息缓冲区

int main()
{
    WSADATA wsaData;
    SOCKET hSock;
    SOCKADDR_IN servAddr;
    HANDLE hSendThread, hRecvThread;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        cerr << "WSAStartup() error" << endl;

    cout << "请输入用户名：";
    cin >> name;

    hSock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
    servAddr.sin_port = htons(PORT);

    if (connect(hSock, (SOCKADDR *)&servAddr, sizeof(servAddr)) != 0)
        cerr << "connect() error" << endl;

    // 创建发送消息线程和接收消息线程
    hSendThread = (HANDLE)_beginthreadex(NULL, 0, SendMsg, (void *)&hSock, 0, NULL);
    hRecvThread = (HANDLE)_beginthreadex(NULL, 0, RecvMsg, (void *)&hSock, 0, NULL);

    // 等待线程结束
    WaitForSingleObject(hSendThread, INFINITE);
    WaitForSingleObject(hRecvThread, INFINITE);

    closesocket(hSock);
    WSACleanup();

    return 0;
}

// 发送消息线程函数
unsigned WINAPI SendMsg(void *arg)
{
    SOCKET hSock = *((SOCKET *)arg);
    char nameMsg[NAME_SIZE + BUF_SIZE];

    while (true)
    {
        cin.getline(msg, BUF_SIZE);

        if (strcmp(msg, "q") == 0 || strcmp(msg, "Q") == 0)
        {
            closesocket(hSock);
            exit(0);
        }

        sprintf(nameMsg, "%s : %s", name, msg);
        send(hSock, nameMsg, strlen(nameMsg), 0);
    }

    return 0;
}

// 接收消息线程函数
unsigned WINAPI RecvMsg(void *arg)
{
    SOCKET hSock = *((SOCKET *)arg);
    char nameMsg[NAME_SIZE + BUF_SIZE];
    int strLen;

    while (true)
    {
        strLen = recv(hSock, nameMsg, NAME_SIZE + BUF_SIZE - 1, 0);

        if (strLen == -1)
            return -1;

        nameMsg[strLen] = '\0';
        cout << nameMsg << "\n";
    }

    return 0;
}

// 错误处理函数
void ErrorHandling(char *msg)
{
    cerr << msg << ", Error Code " << WSAGetLastError() << endl;
    exit(1);
}
