// 服务器端使用非阻塞套接字
#include <iostream>
#include <winsock2.h>
#define PORT 65432     // 定义端口号常量
#define N 10           // N为允许接入的最大客户数
#define BUFFER_LEN 500 // 定义接收缓冲区大小
#pragma comment(lib, "ws2_32.lib")
using namespace std;
int main(int argc, char **argv)
{
    /***定义相关的变量***/
    SOCKET sock_server, newsock[N + 1]; // 定义监听套接字变量及已连接套接字数组，从下标1开始计，下标0不用！
    struct sockaddr_in addr;            // 存放本地地址的sockaddr_in结构变量
    struct sockaddr_in client_addr;     // 存放客户端地址的sockaddr_in结构变量
    char msgbuffer[BUFFER_LEN];         // 定义用于接收客户端发来信息的缓区
    /***发给客户端的信息***/
    char msg[] = "Connect succeed. Please send a message to me.\n";
    /***初始化winsock2.DLL***/
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(2, 2); // 生成版本号2.2
    if (WSAStartup(wVersionRequested, &wsaData) != 0)
    {
        cout << "加载winsock.dll失败！\n";
        return 0;
    }
    /***创建套接字***/
    if ((sock_server = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cout << "创建套接字失败！\n";
        WSACleanup();
        return 0;
    }
    /***设套接字为非阻塞模式***/
    unsigned long ul = 1;
    int nRet = ioctlsocket(sock_server, FIONBIO, &ul); // 设置套接字非阻塞模式
    if (nRet == SOCKET_ERROR)
    {
        cout << "ioctlsocket failed! error:" << WSAGetLastError() << endl;
        closesocket(sock_server);
        WSACleanup();
        return 0;
    }
    /***填写要绑定的本地地址***/
    int addr_len = sizeof(struct sockaddr_in);
    memset((void *)&addr, 0, addr_len);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 允许套接字使用本机任何IP地址
    /***给监听套接字绑定地址***/
    if (bind(sock_server, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        cout << "地址绑定失败！\n";
        closesocket(sock_server);
        WSACleanup();
        return 0;
    }
    /***将套接字设为监听状态****/
    if (listen(sock_server, 0) != 0)
    {
        cout << "listen函数调用失败！\n";
        closesocket(sock_server);
        WSACleanup();
        return 0;
    }
    else
        cout << "listenning......\n";
    int n = 0; // 保存成功接入客户数量的变量
    int err;   // 保存错误代码的变量
    int i, k;  // 循环变量
    /***循环：接收连接请求并收发数据***/
    while (true)
    {
        if (n < N)
        {
            n++;
            if ((newsock[n] = accept(sock_server, (struct sockaddr *)&client_addr, &addr_len)) == INVALID_SOCKET)
            {
                n--;
                err = WSAGetLastError();
                if (err != WSAEWOULDBLOCK)
                {
                    cout << "accept函数调用失败！\n";
                    break; // accept出错终止while循环
                }
            }
            else
            {
                cout << "与" << inet_ntoa(client_addr.sin_addr) << "连接成功！活跃连接数：" << n << endl;
                while (send(newsock[n], msg, sizeof(msg), 0) < 0) // 给客户发送一段信息
                {
                    err = WSAGetLastError();
                    if (err != WSAEWOULDBLOCK)
                    {
                        cout << "数据发送失败！连接断开" << endl;
                        closesocket(newsock[n]); // 关闭出错已连接套接字
                        n--;
                        break;
                    }
                }
            }
        }
        send(newsock[n], msg, sizeof(msg), 0); // 给客户端发送一段信息
        i = 1;
        while (i <= n) // 依次尝试在每一个已连接套接字上接收数据
        {
            memset((void *)msgbuffer, 0, sizeof(msgbuffer));           // 接收缓冲区清零
            if (recv(newsock[i], msgbuffer, sizeof(msgbuffer), 0) < 0) // 接收信息
            {
                err = WSAGetLastError();
                if (err != WSAEWOULDBLOCK)
                {
                    cout << "接收信息失败!" << err << endl;
                    closesocket(newsock[i]); // 关闭出错已连接套接字
                    for (k = i; k < n; k++)
                        newsock[k] = newsock[k + 1];
                    n--;
                }
                else
                    i++;
            }
            else
            {
                getpeername(newsock[i], (struct sockaddr *)&client_addr, &addr_len);
                cout << "The message from " << inet_ntoa(client_addr.sin_addr)
                     << ":" << msgbuffer << endl;
                closesocket(newsock[i]); // 通信完毕关闭已连接套接字
                for (k = i; k < n; k++)
                    newsock[k] = newsock[k + 1];
                n--; // 使newsock数组的前n个元素为未关闭已连接套接字
            }
        }
    }
    /***结束处理***/
    for (i = 1; i < n++; i++)
        closesocket(newsock[i]); // 关闭所有已连接套接字
    closesocket(sock_server);    // 关闭监听套接字
    WSACleanup();                // 注销WinSock动态链接库
    return 0;
}