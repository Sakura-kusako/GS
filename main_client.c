#include <winsock2.h>
#include <stdio.h>
#include <string.h>

#include "gs_socket.h"

GS_Int main(GS_Void)
{
    GS_SockaddrIn_S stAddr;
    GS_SockaddrIn_S stServerAddr;
    GS_SockaddrIn_S stClientAddr;
    GS_Socket s32Sock = -1;
    GS_S32 u32Len = 0;
    GS_S32 s32Len = 0;
    GS_U8 au8Buf[1024] = {0};

    WORD    wVersionRequested;
    WSADATA wsaData;
    int     err;
    wVersionRequested   =   MAKEWORD(2,2);//create 16bit data

    err =   WSAStartup(wVersionRequested,&wsaData); //load win socket
    if(err!=0)
    {
        GS_INFO("Load WinSock Failed!\n");
        return -1;
    }

    s32Sock = GS_SOCK_Socket(GS_AF_INET, GS_SOCK_DGRAM, 0);

    GS_Memset(&stAddr, 0, sizeof(GS_SockaddrIn_S));
    stAddr.sin_family = GS_AF_INET;
    stAddr.sin_port = htons(10086);
    stAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    GS_SOCK_Bind(s32Sock, (GS_Sockaddr_S *)&stAddr, sizeof(GS_SockaddrIn_S));

    GS_Memset(&stServerAddr, 0, sizeof(GS_SockaddrIn_S));
    stServerAddr.sin_family = GS_AF_INET;
    stServerAddr.sin_port = htons(10800);
    stServerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    GS_Memset(&stClientAddr, 0, sizeof(GS_SockaddrIn_S));
    stClientAddr.sin_family = GS_AF_INET;
    stClientAddr.sin_port = htons(10800);
    stClientAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    while (1)
    {
        GS_Memset(au8Buf, 0, sizeof(au8Buf));
        u32Len = sizeof(GS_SockaddrIn_S);
        s32Len = GS_SOCK_Recvfrom(s32Sock, (GS_Char *)au8Buf, sizeof(au8Buf), 0, (GS_Sockaddr_S *)&stAddr, &u32Len);
        GS_INFO("recv len = %d\n", s32Len);
        if (-1 == s32Len)
        {
            GS_INFO("recv error, errno = %d\n", WSAGetLastError());
        }

        if (stAddr.sin_port != htons(10800))
        {
            stClientAddr.sin_port = stAddr.sin_port;
            s32Len = GS_SOCK_Sendto(s32Sock, (GS_Char *)au8Buf, s32Len, 0, (GS_Sockaddr_S *)&stServerAddr, sizeof(GS_SockaddrIn_S));
        }
        else
        {
            s32Len = GS_SOCK_Sendto(s32Sock, (GS_Char *)au8Buf, s32Len, 0, (GS_Sockaddr_S *)&stClientAddr, sizeof(GS_SockaddrIn_S));
        }

        GS_INFO("send len = %d\n", s32Len);
    }

    return 0;
}


