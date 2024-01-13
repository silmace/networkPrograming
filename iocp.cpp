#include <iostream>
#include <winsock2.h>

#define PORT 9990
#define DATA_BUFSIZE 1024

// 定义每个 I/O 操作相关的数据结构
struct PerIoData
{
    OVERLAPPED overlapped;     // I/O 操作的重叠结构
    WSABUF dataBuf;            // 数据缓冲区
    CHAR buffer[DATA_BUFSIZE]; // 实际存储数据的缓冲区
    DWORD bytesSent;           // 已发送的字节数
    DWORD bytesReceived;       // 已接收的字节数
};

// 定义每个句柄相关的数据结构
struct PerHandleData
{
    SOCKET socket; // 套接字句柄
};

// 工作线程函数
DWORD WINAPI ServerWorkerThread(LPVOID completionPortID);

int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed" << std::endl;
        return -1;
    }

    // 创建完成端口
    HANDLE completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (completionPort == NULL)
    {
        std::cerr << "CreateIoCompletionPort failed with error: " << GetLastError() << std::endl;
        return -1;
    }

    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);

    // 创建工作线程
    for (DWORD i = 0; i < systemInfo.dwNumberOfProcessors * 2; i++)
    {
        HANDLE threadHandle = CreateThread(NULL, 0, ServerWorkerThread, completionPort, 0, NULL);
        if (threadHandle == NULL)
        {
            std::cerr << "CreateThread failed with error: " << GetLastError() << std::endl;
            return -1;
        }
        CloseHandle(threadHandle);
    }

    // 创建监听套接字
    SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (listenSocket == INVALID_SOCKET)
    {
        std::cerr << "WSASocket failed with error: " << WSAGetLastError() << std::endl;
        return -1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(PORT);

    // 绑定和监听套接字
    if (bind(listenSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
    {
        std::cerr << "bind failed with error: " << WSAGetLastError() << std::endl;
        return -1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "listen failed with error: " << WSAGetLastError() << std::endl;
        return -1;
    }

    std::cout << "Server started. Listening on port " << PORT << std::endl;

    while (true)
    {
        // 接受连接请求
        SOCKET acceptSocket = WSAAccept(listenSocket, NULL, NULL, NULL, 0);
        if (acceptSocket == INVALID_SOCKET)
        {
            std::cerr << "WSAAccept failed with error: " << WSAGetLastError() << std::endl;
            return -1;
        }

        sockaddr_in clientAddress{};
        int clientAddressLength = sizeof(clientAddress);
        getpeername(acceptSocket, reinterpret_cast<sockaddr *>(&clientAddress), &clientAddressLength);
        std::cout << "Accepted connection from " << inet_ntoa(clientAddress.sin_addr) << ":" << ntohs(clientAddress.sin_port) << std::endl;

        // 创建 I/O 完成端口和相关数据结构
        PerHandleData *perHandleData = new PerHandleData();
        perHandleData->socket = acceptSocket;

        // 将套接字绑定到完成端口
        if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(acceptSocket), completionPort, reinterpret_cast<ULONG_PTR>(perHandleData), 0) == NULL)
        {
            std::cerr << "CreateIoCompletionPort failed with error: " << GetLastError() << std::endl;
            return -1;
        }

        // 创建 I/O 数据结构
        PerIoData *perIoData = new PerIoData();
        ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
        perIoData->bytesSent = 0;
        perIoData->bytesReceived = 0;
        perIoData->dataBuf.len = DATA_BUFSIZE;
        perIoData->dataBuf.buf = perIoData->buffer;

        DWORD flags = 0;
        DWORD bytesReceived = 0;

        // 异步接收数据
        if (WSARecv(acceptSocket, &(perIoData->dataBuf), 1, &bytesReceived, &flags, &(perIoData->overlapped), NULL) == SOCKET_ERROR)
        {
            if (WSAGetLastError() != ERROR_IO_PENDING)
            {
                std::cerr << "WSARecv failed with error: " << WSAGetLastError() << std::endl;
                return -1;
            }
        }
    }

    return 0;
}

// 工作线程函数
DWORD WINAPI ServerWorkerThread(LPVOID completionPortID)
{
    HANDLE completionPort = reinterpret_cast<HANDLE>(completionPortID);

    while (true)
    {
        DWORD bytesTransferred;
        PerHandleData *perHandleData;
        PerIoData *perIoData;

        // 等待 I/O 完成事件
        if (GetQueuedCompletionStatus(completionPort, &bytesTransferred, reinterpret_cast<PULONG_PTR>(&perHandleData), reinterpret_cast<LPOVERLAPPED *>(&perIoData), INFINITE) == 0)
        {
            DWORD lastError = GetLastError();
            if (lastError != ERROR_IO_PENDING)
            {
                std::cerr << "GetQueuedCompletionStatus failed with error: " << lastError << std::endl;
                return 0;
            }
        }

        if (bytesTransferred == 0)
        {
            // 关闭套接字
            std::cout << "Closing socket " << perHandleData->socket << std::endl;
            if (closesocket(perHandleData->socket) == SOCKET_ERROR)
            {
                std::cerr << "closesocket failed with error: " << WSAGetLastError() << std::endl;
                return 0;
            }
            delete perHandleData;
            delete perIoData;
            continue;
        }

        if (perIoData->bytesReceived == 0)
        {
            perIoData->bytesReceived = bytesTransferred;
            perIoData->bytesSent = 0;
        }
        else
        {
            perIoData->bytesSent += bytesTransferred;
        }

        if (perIoData->bytesReceived > perIoData->bytesSent)
        {
            // 处理接收到的数据
            ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
            perIoData->dataBuf.buf = perIoData->buffer + perIoData->bytesSent;
            perIoData->dataBuf.len = perIoData->bytesReceived - perIoData->bytesSent;

            sockaddr_in clientAddress{};
            int clientAddressLength = sizeof(clientAddress);
            getpeername(perHandleData->socket, reinterpret_cast<sockaddr *>(&clientAddress), &clientAddressLength);
            std::cout << "Received from " << inet_ntoa(clientAddress.sin_addr) << ":" << ntohs(clientAddress.sin_port) << " " << perIoData->dataBuf.buf << "\n";

            DWORD bytesSent = 0;
            // 异步发送数据
            if (WSASend(perHandleData->socket, &(perIoData->dataBuf), 1, &bytesSent, 0, &(perIoData->overlapped), NULL) == SOCKET_ERROR)
            {
                if (WSAGetLastError() != ERROR_IO_PENDING)
                {
                    std::cerr << "WSASend failed with error: " << WSAGetLastError() << std::endl;
                    return 0;
                }
            }
            memset(perIoData->dataBuf.buf, 0, perIoData->dataBuf.len);
        }
        else
        {
            perIoData->bytesReceived = 0;
            DWORD flags = 0;
            ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
            perIoData->dataBuf.len = DATA_BUFSIZE;
            perIoData->dataBuf.buf = perIoData->buffer;

            DWORD bytesReceived = 0;
            // 异步接收数据
            if (WSARecv(perHandleData->socket, &(perIoData->dataBuf), 1, &bytesReceived, &flags, &(perIoData->overlapped), NULL) == SOCKET_ERROR)
            {
                if (WSAGetLastError() != ERROR_IO_PENDING)
                {
                    std::cerr << "WSARecv failed with error: " << WSAGetLastError() << std::endl;
                    return 0;
                }
            }
        }
    }
}