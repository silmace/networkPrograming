#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")

#include <iostream>
#include <sstream>

using namespace std;

#define ICMP_MIN 8		   // ICMP包的最小长度为8个字节，只包含包头
#define DEF_PACKET_SIZE 32 // 执行ping操作时指定发送数据包的缺省大小
#define MAX_PACKET 1024	   // 执行ping操作时指定发送数据包的最大大小
#define ICMP_ECHO 8		   // 表示ICMP包为回射请求包
#define ICMP_ECHOREPLY 0   // 表示ICMP包为回射应答包

// IP数据包头结构
typedef struct iphdr
{
	unsigned int h_len : 4;		   // 包头长度
	unsigned int version : 4;	   // IP协议版本
	unsigned char tos;			   // 服务类型(TOS)
	unsigned short total_len;	   // 包的总长度
	unsigned short ident;		   // 包的唯一标识
	unsigned short frag_and_flags; // 标识
	unsigned char ttl;			   // 生存时间（TTL）
	unsigned char proto;		   // 传输协议 (TCP, UDP等)
	unsigned short checksum;	   // IP校验和
	unsigned int sourceIP;		   // 源IP地址
	unsigned int destIP;		   // 目的IP地址
} IpHeader;

// ICMP数据包结构
typedef struct _ihdr
{
	BYTE i_type;	 // 类型
	BYTE i_code;	 // 编码
	USHORT i_cksum;	 // 检验和
	USHORT i_id;	 // 编号
	USHORT i_seq;	 // 序列号
	ULONG timestamp; // 时间戳
} IcmpHeader;

// 计算ICMP包的校验和
USHORT checksum(USHORT *buffer, int size)
{
	unsigned long cksum = 0;
	// 把缓冲区中的数据相加
	while (size > 1)
	{
		cksum += *buffer++;
		size -= sizeof(USHORT);
	}
	if (size)
	{
		cksum += *(UCHAR *)buffer;
	}
	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >> 16);
	return (USHORT)(~cksum);
}

// 对返回的IP数据包进行解码，定位到ICMP数据
// 因为ICMP数据包含在IP数据包中
int decode_resp(char *buf, int bytes, struct sockaddr_in *from, DWORD tid)
{
	IpHeader *iphdr;		 // IP数据包头
	IcmpHeader *icmphdr;	 // ICMP包头
	unsigned short iphdrlen; // IP数据包头的长度
	iphdr = (IpHeader *)buf; // 从buf中IP数据包头的指针，接收的数据

	// 计算IP数据包头的长度
	iphdrlen = iphdr->h_len * 4; // number of 32-bit words *4 = bytes
	// 如果指定的缓冲区长度小于IP包头加上最小的ICMP包长度，则说明它包含的ICMP数据不完整,或者不包含ICMP数据
	if (bytes < iphdrlen + ICMP_MIN)
	{
		return -1;
	}

	// 定位到ICMP包头的起始位置
	icmphdr = (IcmpHeader *)(buf + iphdrlen);
	// 如果ICMP包的类型不是回应包，则不处理
	if (icmphdr->i_type != ICMP_ECHOREPLY)
	{
		return -2; // fprintf(stderr,"non-echo type %d recvd\n",icmphdr->i_type);
	}
	// 发送的ICMP包ID和接收到的ICMP包ID应该对应
	if (icmphdr->i_id != (USHORT)tid)
	{			   //(USHORT)GetCurrentProcessId()) {
		return -3; // fprintf(stderr,"someone else's packet!\n");
	}
	// 返回发送ICMP包和接收回应包的时间差
	int time = GetTickCount() - (icmphdr->timestamp);
	if (time >= 0)
		return time;
	else
		return -4; // 时间值不对
}

// 填充ICMP请求包
inline DWORD fill_icmp_data(char *icmp_data, int datasize, USHORT &seq_no)
{

	IcmpHeader *icmp_hdr;
	char *datapart;
	// 将缓冲区转换为icmp_hdr结构，强制类型转换
	icmp_hdr = (IcmpHeader *)icmp_data;
	// 填充各字段的值
	icmp_hdr->i_type = ICMP_ECHO;				   // 将类型设置为ICMP响应包
	icmp_hdr->i_code = 0;						   // 将编码设置为0
	icmp_hdr->i_id = (USHORT)GetCurrentThreadId(); // 将编号设置为当前线程的编号
	icmp_hdr->i_cksum = 0;						   // 将校验和设置为0
	icmp_hdr->i_seq = 0;						   // 将序列号设置为0
	icmp_hdr->i_cksum = 0;						   // 校验和
	// DWORD startTime = GetTickCount();
	DWORD startTime = GetTickCount(); // 获取当前时间
	icmp_hdr->timestamp = startTime;
	icmp_hdr->i_seq = seq_no++;
	icmp_hdr->i_cksum = checksum((USHORT *)icmp_data, datasize);
	return startTime;
	/*
		//数据部分
		datapart = icmp_data + sizeof(IcmpHeader);				// 定义到数据部分，指针从第sizeof(IcmpHeader)开始
		// 在数据部分随便填充一些数据
		memset(datapart, 'E', datasize - sizeof(IcmpHeader));	//datasize - sizeof(IcmpHeader)就是全部的数据部分
	*/
}

// 执行ping一个IP地址的操作
// 参数ip为IP地址字符串
// 参数timeout指定ping超时时间
int ping(const char *ip, DWORD timeout)
{
	WSADATA wsaData;			   // 初始化Windows Socket的数据
	SOCKET sockRaw = INADDR_ANY;   // 用于执行ping操作的套接字
	struct sockaddr_in dest, from; // socket通信的地址
	struct hostent *hp;			   // 保存主机信息
	int datasize;				   // 发送数据包的大小
	char *dest_ip;				   // 目的地址
	DWORD startTime;			   // 开始时间
	char *icmp_data = NULL;		   // 用来保存ICMP包的数据
	char *recvbuf = NULL;		   // 用来保存应答数据
	USHORT seq_no = 0;			   // ICMP包的序列号
	int ret = -1;				   // 返回值
	int fromlen = sizeof(from);	   // from的长度
	unsigned int addr = 0; 			   // 保存IP地址
	int bread;					   // 接收到的字节数

	// 初始化SOCKET
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		ret = -1000; // WSAStartup 错误
		goto FIN;
	}
	// 创建一个原始套接字
	sockRaw = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	// 如果出现错误，则转到最后
	if (sockRaw == INVALID_SOCKET)
	{
		ret = -2; // WSASocket 错误
		goto FIN;
	}

	// 设置套接字的接收超时选项
	bread = setsockopt(sockRaw, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
	// 如果出现错误，则转到最后
	if (bread == SOCKET_ERROR)
	{
		ret = -3; // setsockopt 错误
		goto FIN;
	}

	// 设置套接字的发送超时选项
	bread = setsockopt(sockRaw, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
	if (bread == SOCKET_ERROR)
	{
		ret = -4; // setsockopt 错误
		goto FIN;
	}
	memset(&dest, 0, sizeof(dest));

	// 将IP地址转换为网络字节序
	hp = gethostbyname(ip); // 获取远程主机的名称
	if (!hp)
	{
		addr = inet_addr(ip);
	}
	if ((!hp) && (addr == INADDR_NONE))
	{
		ret = -5; // 域名错误
		goto FIN;
	}

	// 输出目标IP地址
	if (hp  != NULL)
	{
		in_addr ipAddr;
		cout << endl;
		for (int i = 0;; i++) // 有多个地址信息
		{
			char *p = hp->h_addr_list[i];
			if (p == NULL)
				break;

			// 内存拷贝，从p所指的数据开始，拷贝ph->h_length个字节数
			memcpy(&ipAddr.S_un.S_addr, p, hp->h_length);

			cout << "目标地址IP " << inet_ntoa(ipAddr) << endl;
		}
	}

	// 配置远程通信地址
	if (hp != NULL)
		memcpy(&(dest.sin_addr), hp->h_addr_list[0], hp->h_length);
	else
		dest.sin_addr.s_addr = addr;

	if (hp)
		dest.sin_family = hp->h_addrtype; // IP地址类型
	else
		dest.sin_family = AF_INET;

	dest_ip = inet_ntoa(dest.sin_addr); // 转换为点分十进制

	// 准备要发送的数据
	datasize = DEF_PACKET_SIZE;
	datasize += sizeof(IcmpHeader); // 加上ICMP头后的长度
	char icmp_dataStack[MAX_PACKET];
	char recvbufStack[MAX_PACKET];
	icmp_data = icmp_dataStack; // 用来保存ICMP包的数据	char指针
	recvbuf = recvbufStack;		// 用来保存应答数据		char指针
	// 未能分配到足够的空间
	if (!icmp_data)
	{
		ret = -6; //
		goto FIN;
	}

	memset(icmp_data, 0, MAX_PACKET); // 0填充，ICMP包初始化

	// 设置ICMP报文
	startTime = fill_icmp_data(icmp_data, datasize, seq_no);

	// 发送数据
	// 这里不能写成int bwrote = sendto(sockRaw, icmp_data, datasize, 0, (struct sockaddr*)&dest, sizeof(dest));
	// 会发生错误
	int bwrote;
	// 指定一个socket句柄，包含传输数据的缓冲区，缓冲区长度，指定调用函数的方式，接收数据的目标地址，目标地址长度
	bwrote = sendto(sockRaw, icmp_data, datasize, 0, (struct sockaddr *)&dest, sizeof(dest));
	if (bwrote == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSAETIMEDOUT)
		{
			ret = -7; // 发送错误
			goto FIN;
		}
	}
	if (bwrote < datasize)
	{
		ret = -8; // 发送错误
		goto FIN;
	}
	// 使用QueryPerformance函数用于精确判断结果返回时间值
	// 原有的其他的Windows函数（GetTickCount等）的方式返回值与Windows Ping应用程序相差太大。

	LARGE_INTEGER ticksPerSecond;
	LARGE_INTEGER start_tick;
	LARGE_INTEGER end_tick;
	double elapsed;								// 经过的时间
	QueryPerformanceFrequency(&ticksPerSecond); // CPU 每秒跑几个tick
	QueryPerformanceCounter(&start_tick);		// 开始时系统计数器的位置

	// 接收数据
	
	{
		// 接收回应包
		// socket句柄，接收数据缓冲区，缓冲区长度，调用函数方式，接收地址，地址长度
		bread = recvfrom(sockRaw, recvbuf, MAX_PACKET, 0, (struct sockaddr *)&from, &fromlen);
		if (bread == SOCKET_ERROR)
		{
			if (WSAGetLastError() == WSAETIMEDOUT)
			{
				ret = -1; // 超时
				goto FIN;
			}
			ret = -9; // 接收错误
			goto FIN;
		}
		// 对回应的IP数据包进行解析，定位ICMP数据
		int time = decode_resp(recvbuf, bread, &from, GetCurrentThreadId());
		if (time >= 0)
		{
			// ret = time;
			QueryPerformanceCounter(&end_tick);														 // 获取结束时系统计数器的值
			elapsed = ((double)(end_tick.QuadPart - start_tick.QuadPart) / ticksPerSecond.QuadPart); // 计算ping操作的用时
			ret = (int)(elapsed * 1000);

			cout << "收到来自 " << ip << " " << inet_ntoa(from.sin_addr) << " 的回复：字节=" << DEF_PACKET_SIZE << " 时间=" << ret << endl;
			goto FIN;
		}
		else if (GetTickCount() - startTime >= timeout || GetTickCount() < startTime)
		{
			ret = -1; // 超时
			goto FIN;
		}
	}

FIN:
	// 释放资源
	closesocket(sockRaw);
	WSACleanup();
	// 返回ping操作用时或者错误编号
	return ret;
}

void pingFun(string ip)
{
	// 执行ping操作
	cout << "\n正在 Ping " << ip << endl;
	int ret = ping(ip.c_str(), 500);

	if (ret >= 0)
	{
		cout << ip << "在线" << endl;
	}
	else
	{
		switch (ret)
		{
		case -1:
			printf("ping超时。\n");
			break;
		case -2:
			printf("创建套接字出错。\n");
			break;
		case -3:
			printf("设置套接字的接收超时选项出错。\n");
			break;
		case -4:
			printf("设置套接字的发送超时选项\n");
			break;
		case -5:
			printf("获取域名时出错，可能是IP地址不正确。\n");
			break;
		case -6:
			printf("未能为ICMP数据包分配到足够的空间\n");
			break;
		case -7:
			printf("发送ICMP数据包出错\n");
			break;
		case -8:
			printf("发送ICMP数据包的数量不正确。\n");
			break;
		case -9:
			printf("接收ICMP数据包出错\n");
			break;
		case -1000:
			printf("初始化Windows Sockets环境出错。\n");
			break;
		default:
			printf("未知的错误");
			break;
		}
	}
	printf("\n");
}

int main()
{
	string ip;
	cout << "请输入要Ping的IP地址: ";
	getline(std::cin, ip);
	pingFun(ip);

	return 0;
}