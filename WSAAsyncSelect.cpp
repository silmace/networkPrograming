#include <iostream>
#include <winsock2.h>

using namespace std;

#pragma comment(lib, "ws2_32.lib")
#define WM_SOCKET (WM_USER + 1)

const int BUF_SIZE = 64;
const int SERVER_PORT = 9990;

// Function prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CleanupAndExit(SOCKET sHost, HWND hwnd);
void HandleError(const string &message, SOCKET sHost, HWND hwnd = nullptr);
void HandleAccept(SOCKET sHost, HWND hwnd);
void HandleRead(SOCKET clientSock);

int main()
{
    WSADATA wsd;
    SOCKET sHost;
    SOCKADDR_IN servAddr;
    int retVal;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
    {
        HandleError("WSAStartup", sHost);
    }

    // Create socket
    sHost = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sHost == INVALID_SOCKET)
    {
        HandleError("socket", sHost);
    }

    // Configure server address
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(SERVER_PORT);

    // Bind the socket
    retVal = bind(sHost, (LPSOCKADDR)&servAddr, sizeof(servAddr));
    if (retVal == SOCKET_ERROR)
    {
        HandleError("bind", sHost);
    }

    // Listen for incoming connections
    retVal = listen(sHost, 1);
    if (retVal == SOCKET_ERROR)
    {
        HandleError("listen", sHost);
    }
    cout << "Server is listening on port " << SERVER_PORT << endl;

    // Register window class
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = TEXT("MyClass");

    if (!RegisterClass(&wc))
    {
        HandleError("RegisterClass", sHost);
    }

    // Create a hidden window
    HWND hwnd = CreateWindow(TEXT("MyClass"), TEXT("My Window"), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
    if (!hwnd)
    {
        HandleError("CreateWindow", sHost);
    }

    // Register socket event
    WSAAsyncSelect(sHost, hwnd, WM_SOCKET, FD_ACCEPT);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        // Translate virtual-key messages into character messages
        TranslateMessage(&msg);
        // Send message to WindowProc()
        DispatchMessage(&msg);
    }

    // Cleanup and exit
    CleanupAndExit(sHost, hwnd);

    return 0;
}

// Window procedure callback
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SOCKET:
        if (WSAGETSELECTERROR(lParam))
        {
            cerr << "Socket failed with error: " << WSAGETSELECTERROR(lParam) << endl;
            closesocket(wParam);
            break;
        }

        switch (WSAGETSELECTEVENT(lParam))
        {
        case FD_ACCEPT:
            HandleAccept(wParam, hwnd);
            break;

        case FD_READ:
            HandleRead(wParam);
            break;

        case FD_CLOSE:
            closesocket(wParam);
            break;
        }
        break;

    case WM_DESTROY:
        // PostQuitMessage to exit the message loop
        PostQuitMessage(0);
        break;

    default:
        // Call the default window procedure for other messages
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

// Function to clean up resources and exit the program
void CleanupAndExit(SOCKET sHost, HWND hwnd)
{
    closesocket(sHost);
    WSACleanup();
    if (hwnd)
        DestroyWindow(hwnd);
    exit(0);
}

// handle errors, print error message, and exit the program
void HandleError(const string &message, SOCKET sHost, HWND hwnd)
{
    cerr << message << ": " << WSAGetLastError() << endl;
    CleanupAndExit(sHost, hwnd);
}

// handle incoming connections
void HandleAccept(SOCKET sHost, HWND hwnd)
{
    sockaddr_in clientAddr;
    int addrLen = sizeof(clientAddr);

    SOCKET clientSock = accept(sHost, (sockaddr *)&clientAddr, &addrLen);
    if (clientSock == INVALID_SOCKET)
    {
        HandleError("accept failed", sHost, hwnd);
    }

    cout << "Client connection [" << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << "]" << endl;

    WSAAsyncSelect(clientSock, hwnd, WM_SOCKET, FD_READ | FD_CLOSE);
}

// handle reading data from the client
void HandleRead(SOCKET clientSock)
{
    char buf[BUF_SIZE];
    ZeroMemory(buf, BUF_SIZE);

    sockaddr_in clientAddr;
    int addrLen = sizeof(clientAddr);

    // Retrieve client information
    if (getpeername(clientSock, (sockaddr *)&clientAddr, &addrLen) == SOCKET_ERROR)
    {
        cerr << "getpeername failed: " << WSAGetLastError() << endl;
        closesocket(clientSock);
        return;
    }

    int retVal = recv(clientSock, buf, BUF_SIZE - 1, 0);
    if (retVal == SOCKET_ERROR)
    {
        cerr << "recv failed: " << WSAGetLastError() << endl;
        closesocket(clientSock);
        return;
    }

    buf[retVal] = '\0';
    // cout time D-M-Y H:M:S and client ip and port and message
    SYSTEMTIME st;
    GetLocalTime(&st);
    cout << st.wYear << "-" << st.wMonth << "-" << st.wDay << " " << st.wHour << ":" << st.wMinute << ":" << st.wSecond << " ";
    cout << "Received from [" << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << "] " << buf << endl;

    retVal = send(clientSock, buf, strlen(buf), 0);
    if (retVal == SOCKET_ERROR)
    {
        cerr << "send failed: " << WSAGetLastError() << endl;
        closesocket(clientSock);
    }
}
