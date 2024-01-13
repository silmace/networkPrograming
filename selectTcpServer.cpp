#include <iostream>
#include <winsock2.h>
#include <vector>

using namespace std;
#pragma comment(lib, "ws2_32.lib")

constexpr int BUF_SIZE = 64;              // 缓冲区大小
constexpr int SERVER_PORT = 9990;         // 服务器端口
constexpr char SERVER_IP[] = "127.0.0.1"; // 服务器IP地址
constexpr int MAX_CLIENTS = 10;           // 最大客户端数量

void handleClientData(SOCKET clientSocket, vector<SOCKET>& clientSockets)
{
    char buf[BUF_SIZE];
    ZeroMemory(buf, BUF_SIZE);

    int retVal = recv(clientSocket, buf, BUF_SIZE - 1, 0);
    if (retVal == SOCKET_ERROR)
    {
        cerr << "recv failed: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        return;
    }

    if (retVal == 0)
    {
        cout << "Client disconnected." << endl;
        closesocket(clientSocket);
        return;
    }

    buf[retVal] = '\0';
    SYSTEMTIME st;
    GetLocalTime(&st);
    cout << st.wHour << ":" << st.wMinute << ":" << st.wSecond << " ";
    // cout client ip and port
    sockaddr_in clientAddr{};
    int nSize = sizeof(clientAddr);
    getpeername(clientSocket, reinterpret_cast<sockaddr*>(&clientAddr), &nSize);
    cout << "Received from " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << " " << buf << endl;

    // 处理客户端请求，这里简单地回显数据
    retVal = send(clientSocket, buf, strlen(buf), 0);
    if (retVal == SOCKET_ERROR)
    {
        cerr << "send failed: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        return;
    }
}

void cleanupSockets(const vector<SOCKET>& sockets)
{
    for (auto socket : sockets)
    {
        closesocket(socket);
    }
}

int main()
{
    WSADATA wsd;
    if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
    {
        cerr << "WSAStartup failed: " << WSAGetLastError() << endl;
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET)
    {
        cerr << "socket failed: " << WSAGetLastError() << endl;
        WSACleanup();
        return -1;
    }

    SOCKADDR_IN serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(serverSocket, reinterpret_cast<SOCKADDR*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
    {
        cerr << "bind failed: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        cerr << "listen failed: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "Server is listening on port " << SERVER_PORT << "..." << endl;

    vector<SOCKET> clientSockets;

    while (true)
    {
        fd_set readSet, writeSet, exceptSet;
        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);
        FD_ZERO(&exceptSet);

        FD_SET(serverSocket, &readSet);

        for (const auto& clientSocket : clientSockets)
        {
            FD_SET(clientSocket, &readSet);
            FD_SET(clientSocket, &writeSet);
            FD_SET(clientSocket, &exceptSet);
        }

        int activity = select(0, &readSet, &writeSet, &exceptSet, nullptr);
        if (activity == SOCKET_ERROR)
        {
            cerr << "select failed: " << WSAGetLastError() << endl;
            break;
        }

        if (FD_ISSET(serverSocket, &readSet))
        {
            SOCKET newClient = accept(serverSocket, nullptr, nullptr);
            if (newClient == INVALID_SOCKET)
            {
                cerr << "accept failed: " << WSAGetLastError() << endl;
            }
            else
            {
                // client ip and port
                sockaddr_in clientAddr{};
                int addrLen = sizeof(clientAddr);
                getpeername(newClient, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
                cout << "New client [" << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << "] connected, " << "Total: " << clientSockets.size() + 1 << endl;
                clientSockets.push_back(newClient);
            }
        }

        for (auto it = clientSockets.begin(); it != clientSockets.end();)
        {
            SOCKET clientSocket = *it;

            if (FD_ISSET(clientSocket, &readSet))
            {
                handleClientData(clientSocket, clientSockets);
            }

            ++it;
        }
    }

    cleanupSockets(clientSockets);
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}