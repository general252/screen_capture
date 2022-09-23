#include "udp_client.h"

#include <WS2tcpip.h>

int xop::net_init()
{
    //加载套接字库  
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    wVersionRequested = MAKEWORD(2, 2);

    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.                                  */
        return -1;
    }

    /* Confirm that the WinSock DLL supports 2.2.*/
    /* Note that if the DLL supports versions greater    */
    /* than 2.2 in addition to 2.2, it will still return */
    /* 2.2 in wVersion since that is the version we      */
    /* requested.                                        */

    if (LOBYTE(wsaData.wVersion) != 2 ||
        HIBYTE(wsaData.wVersion) != 2) {
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.                                  */
        WSACleanup();
        return -1;
    }

    return 0;
}

int xop::net_cleanup()
{
    //终止套接字库的使用
    WSACleanup();

    return 0;
}

bool xop::udp_client::Create(const std::string& ip, uint16_t port)
{
    //创建套接字  
    SOCKET sockClient = socket(AF_INET, SOCK_DGRAM, 0);
    if (INVALID_SOCKET == sockClient) {
        printf("socket() called failed! The error code is: %d\n", WSAGetLastError());
        return false;
    }
    else
    {
        printf("socket() called succesful!\n");
    }

    int bufSize = 512 * 1024;//设置为512K
    if (0 != setsockopt(sockClient, SOL_SOCKET, SO_SNDBUF, (const char*)&bufSize, sizeof(int))) {
        printf("setsockopt buf size failed. %d\n", WSAGetLastError());
        return false;
    }


    //unsigned long on = 1;
    //ioctlsocket(sockClient, FIONBIO, &on);


    addrServer.sin_family = AF_INET;
    addrServer.sin_port = htons(port);

    inet_pton(AF_INET, ip.data(), &addrServer.sin_addr);

    // 反转, 测试
    char iPdotdec[20] = { 0 };
    inet_ntop(AF_INET, (void*)&addrServer.sin_addr, iPdotdec, 16);


    this->sockClient = sockClient;

    return true;
}



int xop::udp_client::Send(const char* buf, int32_t size)
{
    int r = sendto(this->sockClient, buf, size, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
    return r;
}

bool xop::udp_client::Close()
{
    closesocket(this->sockClient);

    this->sockClient = 0;
    return true;
}


bool xop::tcp_client::Create(const std::string& ip, uint16_t port)
{
    //创建套接字  
    SOCKET sockClient = socket(AF_INET, SOCK_STREAM, 0);
    if (INVALID_SOCKET == sockClient) {
        printf("socket() called failed! The error code is: %d\n", WSAGetLastError());
        return false;
    }
    else
    {
        printf("socket() called succesful!\n");
    }

    int bufSize = 2 * 1024 * 1024;//设置为512K
    if (0 != setsockopt(sockClient, SOL_SOCKET, SO_SNDBUF, (const char*)&bufSize, sizeof(int))) {
        printf("setsockopt buf size failed. %d\n", WSAGetLastError());
        return false;
    }


    //unsigned long on = 1;
    //ioctlsocket(sockClient, FIONBIO, &on);


    addrServer.sin_family = AF_INET;
    addrServer.sin_port = htons(port);

    inet_pton(AF_INET, ip.data(), &addrServer.sin_addr);

    // 反转, 测试
    char iPdotdec[20] = { 0 };
    inet_ntop(AF_INET, (void*)&addrServer.sin_addr, iPdotdec, 16);


    //向服务器发出连接请求
    int err = connect(sockClient, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
    if (err == SOCKET_ERROR) {
        printf("connect() called failed! The error code is: %d\n", WSAGetLastError());
        return false;
    }
    else
    {
        printf("connect() called successful\n");
    }


    this->sockClient = sockClient;

    return true;
}

int xop::tcp_client::Send(const char* buf, int32_t size)
{
    ::send(this->sockClient, buf, size, 0);
    return 0;
}

bool xop::tcp_client::Close()
{
    closesocket(this->sockClient);

    this->sockClient = 0;
    return true;
}
