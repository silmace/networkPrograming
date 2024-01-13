#define _WINSOCK_DEPRECATED_NO_WARNINGS // 禁用关于 Winsock 废弃 API 的警告
#define _CRT_SECURE_NO_WARNINGS // 禁用关于不安全的 CRT 函数的警告

#include <iostream>
#include <winsock2.h>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

#define ServerPort 9990
#define BUF_SIZE 1024 // Size of message buffer

// Structure to hold client information
typedef struct
{
    SOCKET sClient;
    sockaddr_in addrClient;
} CLIENT_INFO;

// Function to handle client connections
DWORD WINAPI ClientThread(LPVOID lpParam);

int main(int argc, char* argv[])
{
    WSADATA wsaData;
    SOCKET sockServer, sockClient;
    int retVal;

    // Initialize Winsock version 2.2
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cerr << "WSAStartup failed: " << WSAGetLastError() << endl;
        return 1;
    }

    // Create a new socket to listen for client connections
    sockServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == sockServer)
    {
        cerr << "socket failed: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    // set socket to non-blocking mode
    unsigned long ul = 1;
    int nRet = ioctlsocket(sockServer, FIONBIO, &ul);
    if (nRet == SOCKET_ERROR)
    {
        cerr << "ioctlsocket failed: " << WSAGetLastError() << endl;
        closesocket(sockServer);
        WSACleanup();
        return 1;
    }

    // Bind the socket to an address
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(ServerPort);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sockServer, (const struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
    {
        cerr << "bind failed: " << WSAGetLastError() << endl;
        closesocket(sockServer);
        WSACleanup();
        return 1;
    }

    // Start listening for incoming connections
    retVal = listen(sockServer, SOMAXCONN);
    if (SOCKET_ERROR == retVal)
    {
        cerr << "listen failed: " << WSAGetLastError() << endl;
        closesocket(sockServer);
        WSACleanup();
        return 1;
    }
    else
        cout << "Listening on port " << ServerPort << endl;

    // Accept incoming connections and spawn a new thread to handle each one
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    while (1)
    {
        sockClient = accept(sockServer, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (INVALID_SOCKET == sockClient)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) // No data available; wait for next loop
            {
                Sleep(500);
                continue;
            }
            else // Other errors occured; close the socket
            {
                cerr << "accept failed: " << err << endl;
                closesocket(sockServer);
                WSACleanup();
                return -1;
            }
        }
        else
        {
            // Create a new thread and pass the socket handle to it
            CLIENT_INFO* pClientInfo = new CLIENT_INFO;
            pClientInfo->sClient = sockClient;
            pClientInfo->addrClient = clientAddr;
            HANDLE hThread = CreateThread(NULL, 0, ClientThread, (LPVOID)pClientInfo, 0, NULL);
            if (NULL == hThread)
            {
                cerr << "CreateThread failed: " << GetLastError() << endl;
                closesocket(sockClient);
                closesocket(sockServer);
                WSACleanup();
                return 1;
            }
            else
            {
                cout << "Client connected: " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << endl;
            }
        }
    }
    // Close the server socket
    closesocket(sockServer);
    WSACleanup();
    system("pause");
    return 0;
}

// Function to handle client connections
DWORD WINAPI ClientThread(LPVOID lpParam)
{
    CLIENT_INFO* pClientInfo = (CLIENT_INFO*)lpParam;
    SOCKET sockClient = pClientInfo->sClient;
    sockaddr_in clientAddr = pClientInfo->addrClient;
    delete pClientInfo;
    char szBuf[BUF_SIZE];
    int retVal;

    // Receive data from the client
    while (1)
    {
        memset(szBuf, 0, BUF_SIZE);
        retVal = recv(sockClient, szBuf, BUF_SIZE - 1, 0); // Receive up to BUF_SIZE - 1 bytes to leave space for the null terminator
        if (SOCKET_ERROR == retVal)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) // No data available; wait for next loop
            {
                Sleep(500);
                continue;
            }
            else if (err == WSAECONNRESET) // Connection closed by client
            {
                cout << "Client disconnected: " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << endl;
                closesocket(sockClient);
                return 0;
            }
            else // Other errors occured; close the socket
            {
                cerr << "recv failed: " << err << endl;
                closesocket(sockClient);
                return -1;
            }
        }
        else // Data received from client
        {
            szBuf[retVal] = '\0';
            SYSTEMTIME st;
            GetLocalTime(&st);
            char sDateTime[30];
            sprintf(sDateTime, "%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            cout << "Received " << retVal << " bytes from client " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << " " << szBuf << endl;

            // Echo the data back to the client
            retVal = send(sockClient, szBuf, retVal, 0);
            if (SOCKET_ERROR == retVal)
            {
                cerr << "send failed: " << WSAGetLastError() << endl;
                closesocket(sockClient);
                return -1;
            }

            // Check if the client wants to quit
            if (strcmp(szBuf, "quit") == 0)
            {
                cout << "Client quit at: " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << " kill this thread" << endl;
                closesocket(sockClient);
                return 0;
            }
        }
    }
}