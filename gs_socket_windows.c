#include "gs_socket.h"
#include "gs_stdlib.h"
#include "gs_net.h"

#define GS_ERRNO ((GS_S32)WSAGetLastError())

GS_S32 GS_SOCK_Inet_aton(const GS_Char *szInput, GS_U8 *pu8IPv4)
{
    GS_S32 dots = 0;
    GS_U32 val = 0;
    GS_Char c = 0;

    do
    {
        c = *szInput;
        switch (c)
        {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                val = (val * 10) + (c - '0');
                break;
            case '.':
                if (++dots > 3)
                    return GS_FAILED;
            case '\0':
                if (val > 255)
                    return GS_FAILED;
                *pu8IPv4 = (GS_U8)val;
                pu8IPv4++;
                val = 0;
                break;
            default:
                return GS_FAILED;
        }
    }while (*szInput++);

    if (dots != 3)
        return GS_FAILED;
    return GS_SUCCESS;
}

GS_S32 GS_SOCK_Init(GS_Void)
{
    WORD    wVersionRequested;
    WSADATA wsaData;
    GS_Int  err;
    wVersionRequested = MAKEWORD(2,2);//create 16bit data

    err = WSAStartup(wVersionRequested, &wsaData); //load win socket
    if (err != 0)
    {
        GS_ERROR("WSAStartup Failed!");
        err = GS_FAILED;
    }

    return err;
}

GS_S32 GS_SOCK_DeInit(GS_Void)
{
    WSACleanup();
    return GS_SUCCESS;
}

GS_S32 GS_SOCK_Socket(GS_S32 s32Domain, GS_S32 type, GS_S32 s32Protocol)
{
    GS_S32 s32Sock = -1;

    s32Sock = socket(s32Domain, type, s32Protocol);
    if (s32Sock < 0)
    {
        GS_ERROR("socket error, GS_ERRNO = %d", GS_ERRNO);
    }
    else
    {
        // 避免UDP地址不可达时产生10054错误
        if (GS_SOCK_DGRAM == type)
        {
            #define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)

            BOOL bNewBehavior = FALSE;
            DWORD dwBytesReturned = 0;
            WSAIoctl(s32Sock, SIO_UDP_CONNRESET, &bNewBehavior, sizeof bNewBehavior, NULL, 0, &dwBytesReturned, NULL, NULL);
        }
    }

    return s32Sock;
}

GS_S32 GS_SOCK_Bind(GS_S32 s32Sock, const GS_Sockaddr_S *pstAddr, GS_U32 u32AddrLen)
{
    GS_S32 s32Ret = GS_SUCCESS;

    s32Ret = bind(s32Sock, pstAddr, u32AddrLen);
    if (s32Ret < 0)
    {
        GS_ERROR("bind error, GS_ERRNO = %d", GS_ERRNO);
    }

    return s32Ret;
}

GS_S32 GS_SOCK_Connect(GS_S32 s32Sock, const GS_Sockaddr_S *pstAddr, GS_U32 u32AddrLen)
{
    GS_S32 s32Ret = GS_SUCCESS;

    s32Ret = connect(s32Sock, pstAddr, u32AddrLen);
    if (s32Ret < 0)
    {
        GS_ERROR("connect error, GS_ERRNO = %d", GS_ERRNO);
    }

    return s32Ret;
}

GS_S32 GS_SOCK_Recv(GS_S32 s32Sock, GS_U8 *pu8Buf, GS_U32 u32Len, GS_S32 s32Flag)
{
    GS_S32 s32Ret = GS_SUCCESS;

    s32Ret = recv(s32Sock, (char *)pu8Buf, u32Len, s32Flag);
    if (s32Ret < 0)
    {
        //GS_ERROR("recv error, GS_ERRNO = %d", GS_ERRNO);
    }

    return s32Ret;
}

GS_S32 GS_SOCK_Recvfrom(GS_S32 s32Sock, GS_U8 *pu8Buf, GS_U32 u32Len, GS_S32 s32Flag, GS_Sockaddr_S *pstAddr, GS_U32 *pu32AddrLen)
{
    GS_S32 s32Ret = GS_SUCCESS;

    s32Ret = recvfrom(s32Sock, (char *)pu8Buf, u32Len, s32Flag, pstAddr, (int *)pu32AddrLen);
    if (s32Ret < 0)
    {
        //GS_ERROR("recvfrom error, GS_ERRNO = %d", GS_ERRNO);
    }

    return s32Ret;
}

GS_S32 GS_SOCK_SetRecvTimeout(GS_S32 s32Sock, GS_U32 u32TimeMs)
{
    GS_S32 s32Ret = GS_SUCCESS;

    s32Ret = setsockopt(s32Sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&u32TimeMs, sizeof(u32TimeMs));
    if (s32Ret < 0)
    {
        GS_ERROR("setsockopt error, GS_ERRNO = %d", GS_ERRNO);
    }

    return s32Ret;
}

GS_S32 GS_SOCK_Send(GS_S32 s32Sock, GS_U8 *pu8Buf, GS_U32 u32Len, GS_S32 s32Flag)
{
    GS_S32 s32Ret = GS_SUCCESS;

    s32Ret = send(s32Sock, (char *)pu8Buf, u32Len, s32Flag);
    if (s32Ret != u32Len)
    {
        GS_ERROR("send error, GS_ERRNO = %d, (%d) != (%u)", GS_ERRNO, s32Ret, u32Len);
    }

    return s32Ret;
}

GS_S32 GS_SOCK_Sendto(GS_S32 s32Sock, GS_U8 *pu8Buf, GS_U32 u32Len, GS_S32 s32Flag, GS_Sockaddr_S *pstAddr, GS_U32 u32AddrLen)
{
    GS_S32 s32Ret = GS_SUCCESS;

    s32Ret = sendto(s32Sock, (char *)pu8Buf, u32Len, s32Flag, pstAddr, u32AddrLen);
    if (s32Ret != u32Len)
    {
        GS_ERROR("sendto error, GS_ERRNO = %d, (%d) != (%u)", GS_ERRNO, s32Ret, u32Len);
    }

    return s32Ret;
}

GS_S32 GS_SOCK_Select(GS_S32 *ps32Sock, GS_U32 u32Count, GS_U32 u32TimeMs)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_S32 i = 0;
    fd_set fdread;
    struct timeval tv;

    FD_ZERO(&fdread); //初始化fd_set
    for (i=0; i<u32Count; i++)
    {
        FD_SET(ps32Sock[i], &fdread); //分配套接字句柄到相应的fd_set
    }

    tv.tv_sec = u32TimeMs / 1000;
    tv.tv_usec = u32TimeMs % 1000 * 1000;

    s32Ret = select(0, &fdread, NULL, NULL, &tv);
    if (s32Ret < 0)
    {
        s32Ret = -1;
        GS_ERROR("select error, GS_ERRNO = %d", GS_ERRNO);
    }
    else if (s32Ret > 0)
    {
        for (i=0; i<u32Count; i++)
        {
            if (FD_ISSET(ps32Sock[i], &fdread))
            {
                s32Ret = i;
                break;
            }
        }

        if (i == u32Count)
        {
            GS_ERROR("select ret = %d, GS_ERRNO = %d", s32Ret, GS_ERRNO);
            s32Ret = -2;
        }
    }
    else
    {
        // timeout ?
        s32Ret = -2;
    }

    return s32Ret;
}

GS_S32 GS_SOCK_Close(GS_S32 s32Sock)
{
    GS_S32 s32Ret = GS_SUCCESS;

    s32Ret = closesocket(s32Sock);
    if (s32Ret < 0)
    {
        GS_ERROR("closesocket error, GS_ERRNO = %d", GS_ERRNO);
    }

    return s32Ret;
}

GS_S32 GS_SOCK_GetLastError(GS_Void)
{
    return GS_ERRNO;
}

GS_S32 GS_SOCK_StrToIPv4(GS_Char *szStr, GS_U8 *pu8IPv4, GS_U16 *pu16Port)
{
    GS_Char *pcTmp = GS_NULL;

    if (!szStr || !pu8IPv4 || !pu16Port)
    {
        GS_ERROR("bad para");
        return GS_FAILED;
    }

    pcTmp = GS_Strchr(szStr, ':');
    if (!pcTmp)
    {
        GS_ERROR("bad para");
        return GS_FAILED;
    }

    *pcTmp = '\0';
    pcTmp++;

    if (GS_SUCCESS != GS_SOCK_Inet_aton(szStr, pu8IPv4))
    {
        GS_ERROR("bad para");
        return GS_FAILED;
    }

    *pu16Port = GS_Atoi(pcTmp);

    return GS_SUCCESS;
}


