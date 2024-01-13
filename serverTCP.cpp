#include <iostream>
#include <winsock2.h>

using namespace std;
#pragma comment(lib, "ws2_32.lib")

#define PORT 9990   // 服务器端口
#define BUF_SIZE 64 // 缓冲区大小

int main(int argc, char *argv[])
{
    WSADATA wsd;             // 初始化WSADATA
    SOCKET sServer, sClient; // 创建套接字
    int retVal;              // 返回值
    char buf[BUF_SIZE];      // 缓冲区

    // 初始化WinSock
    if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
    {
        cerr << "WSAStartup failed: " << WSAGetLastError() << endl;
        return 1;
    }

    // 创建套接字
    sServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == sServer)
    {
        cerr << "socket failed: " << WSAGetLastError() << endl;
        WSACleanup();
        return -1;
    }

    // 设置服务器Socket地址
    SOCKADDR_IN addrServ;
    addrServ.sin_family = AF_INET;
    addrServ.sin_port = htons(PORT);
    addrServ.sin_addr.s_addr = htonl(INADDR_ANY);

    // 绑定Socket地址
    retVal = bind(sServer, (const struct sockaddr *)&addrServ, sizeof(SOCKADDR_IN));
    if (SOCKET_ERROR == retVal)
    {
        cerr << "bind failed: " << WSAGetLastError() << endl;
        closesocket(sServer);
        WSACleanup();
        return -1;
    }

    // 开始监听
    retVal = listen(sServer, 1);
    if (SOCKET_ERROR == retVal)
    {
        cerr << "listen failed: " << WSAGetLastError() << endl;
        closesocket(sServer);
        WSACleanup();
        return -1;
    }

    // 接受客户端连接
    cout << "TCP Server is listening on port " << PORT << " ..." << endl;
    SOCKADDR_IN addrClient;
    int addrClientlen = sizeof(addrClient);
    sClient = accept(sServer, (SOCKADDR *)&addrClient, &addrClientlen);
    if (INVALID_SOCKET == sClient)
    {
        cerr << "accept failed: " << WSAGetLastError() << endl;
        closesocket(sServer);
        WSACleanup();
        return -1;
    }

    // 与客户端通信
    while (true)
    {
        // 接收数据
        ZeroMemory(buf, BUF_SIZE); // 清空缓冲区
        retVal = recv(sClient, buf, BUF_SIZE, 0);
        if (retVal == SOCKET_ERROR)
        {
            cerr << "recv failed: " << WSAGetLastError() << endl;
            closesocket(sClient);
            closesocket(sServer);
            WSACleanup();
            return -1;
        }

        // 获取当前系统时间
        SYSTEMTIME sys;
        GetLocalTime(&sys);
        char sDateTime[30];
        sprintf(sDateTime, "%d-%d-%d %d:%d:%d ", sys.wYear, sys.wMonth, sys.wDay, sys.wHour, sys.wMinute, sys.wSecond);

        // 显示客户端发送的数据
        cout << sDateTime << "Received from Client [" << inet_ntoa(addrClient.sin_addr) << ":" << ntohs(addrClient.sin_port) << "]：" << buf << endl;

        // 如果客户端发送了"quit"，则服务器退出
        if (strcmp(buf, "quit") == 0)
        {
            retVal = send(sClient, "quit", strlen("quit"), 0);
            break;
        }
        else
        { // 否则，服务器发送数据给客户端
            char msg[BUF_SIZE];
            sprintf(msg, "Server received: %s", buf);
            retVal = send(sClient, msg, strlen(msg), 0);
            if (retVal == SOCKET_ERROR)
            {
                cerr << "send failed: " << WSAGetLastError() << endl;
                closesocket(sClient);
                closesocket(sServer);
                WSACleanup();
                return -1;
            }
        }
    }

    // 关闭套接字
    closesocket(sClient);
    closesocket(sServer);

    // 终止Winsock
    WSACleanup();

    return 0;
}
