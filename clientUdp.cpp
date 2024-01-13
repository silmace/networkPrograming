#include <iostream>
#include <winsock2.h>

using namespace std;

#pragma comment(lib, "ws2_32.lib")

constexpr int BUF_SIZE = 64;              // 缓冲区大小
constexpr int SERVER_PORT = 9990;         // 服务器端口
constexpr char SERVER_IP[] = "127.0.0.1"; // 服务器IP地址

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

// 创建UDP客户端套接字
SOCKET CreateUDPClientSocket()
{
    SOCKET sClient = socket(AF_INET, SOCK_DGRAM, 0); // 使用UDP协议
    if (INVALID_SOCKET == sClient)
    {
        cerr << "socket failed: " << WSAGetLastError() << endl;
    }
    return sClient;
}

// 与服务器通信
void CommunicateWithServer(SOCKET sClient, SOCKADDR_IN &servAddr)
{
    char buf[BUF_SIZE]; // 缓冲区

    while (true)
    {
        cout << "Please input a string to send (type 'quit' to exit): ";
        string str;
        cin >> str;

        ZeroMemory(buf, BUF_SIZE);                 // 清空缓冲区
        strncpy_s(buf, str.c_str(), BUF_SIZE - 1); // 将字符串复制到缓冲区

        // 发送数据到服务器
        int retVal = sendto(sClient, buf, strlen(buf), 0, (SOCKADDR *)&servAddr, sizeof(servAddr));
        if (retVal == SOCKET_ERROR)
        {
            cerr << "sendto failed: " << WSAGetLastError() << endl;
            break;
        }

        // 接收服务器数据
        ZeroMemory(buf, BUF_SIZE); // 清空缓冲区
        retVal = recvfrom(sClient, buf, BUF_SIZE, 0, NULL, NULL);
        if (retVal == SOCKET_ERROR)
        {
            cerr << "recvfrom failed: " << WSAGetLastError() << endl;
            break;
        }
        buf[retVal] = '\0'; // 确保接收的数据以 null 结尾
        cout << "Received from Server: " << buf << endl;

        // 如果输入 "quit"，退出循环
        if (strcmp(buf, "quit") == 0)
        {
            cout << "quit" << endl;
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    WSADATA wsd;          // 初始化WSADATA
    SOCKET sClient;       // 创建套接字
    SOCKADDR_IN servAddr; // 服务器地址

    if (!InitializeWinSock(wsd))
    {
        return 1;
    }

    sClient = CreateUDPClientSocket();
    if (sClient == INVALID_SOCKET)
    {
        WSACleanup();
        return -1;
    }

    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(SERVER_IP); // 服务器地址
    servAddr.sin_port = htons(SERVER_PORT);

    CommunicateWithServer(sClient, servAddr);

    closesocket(sClient); // 关闭套接字
    WSACleanup();         // 清理Winsock

    return 0;
}
