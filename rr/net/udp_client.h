#pragma once


#include <WinSock2.h>
#include <Windows.h>

#include <string>

namespace xop
{

    class udp_client
    {
    public:
        bool Create(const std::string& ip, uint16_t port);
        int Send(const char* buf, int32_t size);
        bool Close();

    private:
        SOCKET sockClient = 0;
        SOCKADDR_IN addrServer;
    };

    class tcp_client
    {
    public:
        bool Create(const std::string& ip, uint16_t port);
        int Send(const char* buf, int32_t size);
        bool Close();

    private:
        SOCKET sockClient = 0;
        SOCKADDR_IN addrServer;
    };

    int net_init();
    int net_cleanup();
}

