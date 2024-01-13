#include <iostream>
#include <winsock2.h>

using namespace std;

#pragma comment(lib, "ws2_32.lib")

constexpr int BUF_SIZE = 64;
constexpr int SERVER_PORT = 9990;

// 初始化WinSock
bool InitializeWinSock(WSADATA &wsd)
{
    if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
    {
        cerr << "WSAStartup failed: " << WSAGetLastError() << endl;
        return false;
    }
    return true;
}

// 创建UDP套接字
SOCKET CreateUDPServerSocket()
{
    SOCKET sServer = socket(AF_INET, SOCK_DGRAM, 0); // 使用UDP协议
    if (INVALID_SOCKET == sServer)
    {
        cerr << "socket failed: " << WSAGetLastError() << endl;
    }
    return sServer;
}

// 绑定套接字到地址
bool BindSocketToAddress(SOCKET sServer, SOCKADDR_IN &servAddr)
{
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); // 接受来自任何地址的数据
    servAddr.sin_port = htons(SERVER_PORT);

    int retVal = bind(sServer, (LPSOCKADDR)&servAddr, sizeof(servAddr));
    if (SOCKET_ERROR == retVal)
    {
        cerr << "bind failed: " << WSAGetLastError() << endl;
        return false;
    }
    return true;
}

// 与客户端通信
void CommunicateWithClients(SOCKET sServer)
{
    char buf[BUF_SIZE]; // 缓冲区

    cout << "UDP Server is listening on port " << SERVER_PORT << " ..." << endl;

    while (true)
    {
        SOCKADDR_IN clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        // 接收数据
        ZeroMemory(buf, BUF_SIZE); // 清空缓冲区
        int retVal = recvfrom(sServer, buf, BUF_SIZE, 0, (SOCKADDR *)&clientAddr, &clientAddrSize);
        if (retVal == SOCKET_ERROR)
        {
            cerr << "recvfrom failed: " << WSAGetLastError() << endl;
            break;
        }

        // 获取当前系统时间
        SYSTEMTIME sys;
        GetLocalTime(&sys);
        char sDateTime[30];
        sprintf(sDateTime, "%d-%d-%d %d:%d:%d ", sys.wYear, sys.wMonth, sys.wDay, sys.wHour, sys.wMinute, sys.wSecond);

        // 显示客户端发送的数据和地址
        cout << sDateTime << "Received From Client [" << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << "]：" << buf << endl;

        // 如果客户端发送了"quit"，则服务器退出
        if (strcmp(buf, "quit") == 0)
        {
            retVal = sendto(sServer, "quit", strlen("quit"), 0, (SOCKADDR *)&clientAddr, sizeof(clientAddr));
            break;
        }
        else
        {
            // 发送数据到客户端
            char msg[BUF_SIZE];
            sprintf(msg, "Server received: %s", buf);
            retVal = sendto(sServer, msg, strlen(msg), 0, (SOCKADDR *)&clientAddr, sizeof(clientAddr));
            if (retVal == SOCKET_ERROR)
            {
                cerr << "sendto failed: " << WSAGetLastError() << endl;
                break;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    WSADATA wsd;          // 初始化WSADATA
    SOCKET sServer;       // 创建套接字
    SOCKADDR_IN servAddr; // 服务器地址

    if (!InitializeWinSock(wsd))
    {
        return 1;
    }

    sServer = CreateUDPServerSocket();
    if (sServer == INVALID_SOCKET)
    {
        WSACleanup();
        return -1;
    }

    if (!BindSocketToAddress(sServer, servAddr))
    {
        closesocket(sServer);
        WSACleanup();
        return -1;
    }

    CommunicateWithClients(sServer);

    closesocket(sServer); // 关闭套接字
    WSACleanup();         // 清理Winsock

    return 0;
}
