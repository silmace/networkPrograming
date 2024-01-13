#include <Winsock2.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
#define SERVER_PORT 9990

// Function to initialize Windows Sockets environment
void InitializeWSA()
{
    WSADATA ws;
    if (WSAStartup(MAKEWORD(2, 2), &ws) != 0)
    {
        cerr << "Failed to initialize Windows Sockets." << endl;
        exit(1);
    }
}

// Function to create and bind the listening socket
SOCKET CreateListenSocket()
{
    SOCKET listenSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET)
    {
        cerr << "Failed to create listening socket." << endl;
        exit(1);
    }

    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(SERVER_PORT);
    sin.sin_addr.S_un.S_addr = INADDR_ANY;

    if (bind(listenSocket, (struct sockaddr *)&sin, sizeof(struct sockaddr)) == SOCKET_ERROR)
    {
        cerr << "Failed to bind listening socket." << endl;
        closesocket(listenSocket);
        exit(1);
    }

    return listenSocket;
}

// Function to create and register event object for a socket
WSAEVENT CreateAndRegisterEvent(SOCKET socket, long networkEvents)
{
    WSAEVENT event = ::WSACreateEvent();
    if (event == WSA_INVALID_EVENT)
    {
        cerr << "Failed to create event object." << endl;
        closesocket(socket);
        exit(1);
    }

    if (WSAEventSelect(socket, event, networkEvents) == SOCKET_ERROR)
    {
        cerr << "Failed to register event object." << endl;
        closesocket(socket);
        WSACloseEvent(event);
        exit(1);
    }

    return event;
}

// Function to accept a new client connection
SOCKET AcceptClientConnection(SOCKET listenSocket)
{
    SOCKET newSocket = accept(listenSocket, NULL, NULL);
    if (newSocket == INVALID_SOCKET)
    {
        cerr << "Failed to accept client connection." << endl;
        closesocket(listenSocket);
        exit(1);
    }

    return newSocket;
}

// Function to handle the FD_ACCEPT event
void HandleAcceptEvent(SOCKET listenSocket, WSAEVENT eventArray[], SOCKET sockArray[], int& eventTotal)
{
    SOCKET newSocket = AcceptClientConnection(listenSocket);
    WSAEVENT newEvent = CreateAndRegisterEvent(newSocket, FD_READ | FD_CLOSE | FD_WRITE);

    eventArray[eventTotal] = newEvent;
    sockArray[eventTotal] = newSocket;
    eventTotal++;

    sockaddr_in clientAddr;
    int addrLen = sizeof(clientAddr);
    getpeername(newSocket, (struct sockaddr *)&clientAddr, &addrLen);
    cout << "New client [" << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << "] connected, " << "Total: " << eventTotal - 1 << endl;
}

// Function to handle the FD_READ event
void HandleReadEvent(SOCKET socket)
{
    char text[256];
    int recvSize = recv(socket, text, sizeof(text), 0);
    if (recvSize > 0)
    {
        text[recvSize] = '\0';

        SYSTEMTIME sys;
        GetLocalTime(&sys);
        cout << sys.wYear << "-" << sys.wMonth << "-" << sys.wDay << " " << sys.wHour << ":" << sys.wMinute << ":" << sys.wSecond;

        sockaddr_in clientAddr;
        int addrLen = sizeof(clientAddr);
        getpeername(socket, (struct sockaddr *)&clientAddr, &addrLen);
        cout << " Received from [" << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << "]  " << text << endl;

        int sendSize = send(socket, text, strlen(text), 0);
        if (sendSize == SOCKET_ERROR)
        {
            cerr << "Failed to send message to client." << endl;
        }
    }
}

// Function to handle the FD_CLOSE event
void HandleCloseEvent(SOCKET socket, WSAEVENT eventArray[], SOCKET sockArray[], int& eventTotal, int index)
{
    closesocket(socket);

    for (int j = index; j < eventTotal - 1; j++)
    {
        sockArray[j] = sockArray[j + 1];
        eventArray[j] = eventArray[j + 1];
    }

    eventTotal--;

    sockaddr_in clientAddr;
    int addrLen = sizeof(clientAddr);
    getpeername(socket, (struct sockaddr *)&clientAddr, &addrLen);
    cout << "Client [" << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << "] disconnected, " << "Total: " << eventTotal - 1 << endl;
}

int main(int argc, char *argv[])
{
    InitializeWSA();

    SOCKET listenSocket = CreateListenSocket();
    listen(listenSocket, 5);

    cout << "Server start listening on port " << SERVER_PORT << endl;

    WSAEVENT eventArray[WSA_MAXIMUM_WAIT_EVENTS];
    SOCKET sockArray[WSA_MAXIMUM_WAIT_EVENTS];
    int eventTotal = 0;

    WSAEVENT listenEvent = CreateAndRegisterEvent(listenSocket, FD_ACCEPT | FD_CLOSE);
    eventArray[eventTotal] = listenEvent;
    sockArray[eventTotal] = listenSocket;
    eventTotal++;

    while (true)
    {
        int index = WSAWaitForMultipleEvents(eventTotal, eventArray, false, WSA_INFINITE, false);
        index = index - WSA_WAIT_EVENT_0;

        for (int i = index; i < eventTotal; i++)
        {
            int ret = WSAWaitForMultipleEvents(1, &eventArray[i], true, 1000, false);
            if (ret == WSA_WAIT_FAILED || ret == WSA_WAIT_TIMEOUT)
            {
                continue;
            }
            else
            {
                WSANETWORKEVENTS event;
                WSAEnumNetworkEvents(sockArray[i], eventArray[i], &event);

                if (event.lNetworkEvents & FD_ACCEPT)
                {
                    if (event.iErrorCode[FD_ACCEPT_BIT] == 0)
                    {
                        if (eventTotal > WSA_MAXIMUM_WAIT_EVENTS)
                        {
                            cout << "Too many connections, temporarily not processing" << endl;
                            continue;
                        }

                        HandleAcceptEvent(listenSocket, eventArray, sockArray, eventTotal);
                    }
                }
                else if (event.lNetworkEvents & FD_READ)
                {
                    if (event.iErrorCode[FD_READ_BIT] == 0)
                    {
                        HandleReadEvent(sockArray[i]);
                    }
                }
                else if (event.lNetworkEvents & FD_CLOSE)
                {
                    if (event.iErrorCode[FD_CLOSE_BIT] == 0)
                    {
                        HandleCloseEvent(sockArray[i], eventArray, sockArray, eventTotal, i);
                    }
                }
            }
        }
    }

    return 0;
}