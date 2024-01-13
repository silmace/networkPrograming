// 客户端是之前的拥塞式的
#define _WINSOCK_DEPRECATED_NO_WARNINGS // 禁用关于 Winsock 废弃 API 的警告
#define _CRT_SECURE_NO_WARNINGS // 禁用关于不安全的 CRT 函数的警告

// Link with the Winsock library
#pragma comment(lib, "Ws2_32.lib")

#include <iostream>
#include <winsock2.h>

using namespace std;

#define ServerPort 9999
#define ServerAddr "127.0.0.1"

int main()
{
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        cerr << "WSAStartup failed: " << result << endl;
        return 1;
    }

    // Create a socket
    SOCKET sSock = socket(AF_INET, SOCK_STREAM, 0);
    if (sSock == INVALID_SOCKET)
    {
        cerr << "socket failed: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    // Connect to the server
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(ServerPort);
    serverAddr.sin_addr.s_addr = inet_addr(ServerAddr);
    if (connect(sSock, (const struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        cerr << "connect failed: " << WSAGetLastError() << endl;
        closesocket(sSock);
        WSACleanup();
        return 1;
    }

    // Send messages to the server
    const int bufSize = 1024;
    char buf[bufSize];
    while (1)
    {
        cout << "Enter a message (or 'quit' to exit): ";
        cin.getline(buf, bufSize);
        int nRet = send(sSock, buf, strlen(buf), 0);
        if (nRet == SOCKET_ERROR)
        {
            cerr << "send failed: " << WSAGetLastError() << endl;
            closesocket(sSock);
            WSACleanup();
            return 1;
        }
        if (strcmp(buf, "quit") == 0)
        {
            break;
        }
    }

    // Close the socket and clean up
    closesocket(sSock);
    WSACleanup();

    return 0;
}