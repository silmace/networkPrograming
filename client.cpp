#include <iostream>
#include <winsock2.h>

using namespace std;
#pragma comment(lib, "ws2_32.lib")

constexpr int BUF_SIZE = 64;              // 缓冲区大小
constexpr int SERVER_PORT = 9990;         // 服务器端口
constexpr char SERVER_IP[] = "127.0.0.1"; // 服务器IP地址

int main(int argc, char *argv[])
{
    WSADATA wsd;          // 初始化WSADATA
    SOCKET sHost;         // 创建套接字
    SOCKADDR_IN servAddr; // 服务器地址
    char buf[BUF_SIZE];   // 缓冲区
    int retVal;           // 返回值

    // 初始化WinSock
    if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
    {
        cerr << "WSAStartup failed: " << WSAGetLastError() << endl;
        return 1;
    }

    // 创建套接字
    sHost = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == sHost)
    {
        cerr << "socket failed: " << WSAGetLastError() << endl;
        WSACleanup();
        return -1;
    }

    // 设置服务器地址
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    servAddr.sin_port = htons(SERVER_PORT);

    // 连接服务器
    retVal = connect(sHost, (LPSOCKADDR)&servAddr, sizeof(servAddr));
    if (SOCKET_ERROR == retVal)
    {
        cerr << "connect failed: " << WSAGetLastError() << endl;
        closesocket(sHost);
        WSACleanup();
        return 1;
    }

    // 与服务器通信
    while (true)
    {
        cout << "Please input a string to send (type 'quit' to exit): ";
        string str;
        cin >> str;

        ZeroMemory(buf, BUF_SIZE);                 // 清空缓冲区
        strncpy_s(buf, str.c_str(), BUF_SIZE - 1); // 将字符串复制到缓冲区

        // 发送数据
        retVal = send(sHost, buf, strlen(buf), 0);
        if (SOCKET_ERROR == retVal)
        {
            cerr << "send failed: " << WSAGetLastError() << endl;
            break;
        }

        // 接收数据
        ZeroMemory(buf, BUF_SIZE); // 清空缓冲区
        retVal = recv(sHost, buf, BUF_SIZE - 1, 0);
        if (retVal == SOCKET_ERROR)
        {
            cerr << "recv failed: " << WSAGetLastError() << endl;
            break;
        }
        buf[retVal] = '\0'; // 确保接收的数据以 null 结尾
        cout << "Received from Server: " << buf << endl;

        if (strcmp(buf, "quit") == 0)
        {
            cout << "Server has quit. Exiting..." << endl;
            break;
        }
    }

    closesocket(sHost); // 关闭套接字
    WSACleanup();       // 清理Winsock

    return 0;
}
