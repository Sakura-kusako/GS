#ifndef _GS_SOCKET_H_
#define _GS_SOCKET_H_

#include "gs_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef GS_PLATFORM_LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#define GS_ERRNO_AGAIN      EAGAIN
#define GS_ERRNO_INTR       EINTR
#define GS_ERRNO_TIMEOUT    EAGAIN
#endif
#ifdef GS_PLATFORM_WINDOWS
#include <winsock2.h>

#define GS_ERRNO_AGAIN      WSATRY_AGAIN
#define GS_ERRNO_INTR       WSAEINTR
#define GS_ERRNO_TIMEOUT    WSAETIMEDOUT
#endif

#define GS_AF_INET      AF_INET
#define GS_SOCK_DGRAM   SOCK_DGRAM


#ifdef GS_LITTLE_ENDIAN
#define GS_Htonl(x)     GS_SWAP_ENDIAN_32(x)
#define GS_Htons(x)     GS_SWAP_ENDIAN_16(x)
#else
#define GS_Htonl(x)     ((GS_U32)(x))
#define GS_Htons(x)     ((GS_U16)(x))
#endif

#define GS_Ntohl(x)     GS_Htonl(x)
#define GS_Ntohs(x)     GS_Htons(x)

#define GS_SOCK_SetIPv4(ip,a,b,c,d) ((ip)[0]=(a),(ip)[1]=(b),(ip)[2]=(c),(ip)[3]=(d))
#define GS_SOCK_SetIPv4Local(ip)    GS_SOCK_SetIPv4(ip,127,0,0,1)

typedef int                 GS_Socket;
typedef struct sockaddr     GS_Sockaddr_S;
typedef struct
{
    GS_S16  s16Family;  ///< GS_AF_INET
    GS_U16  u16Port;    ///< GS_Htons(u16Port)
    GS_U8   au8IPv4[4]; ///< {127, 0, 0, 1}
    GS_U8   au8Zero[8]; ///< 
} GS_SockaddrIn_S;

GS_S32 GS_SOCK_Init(GS_Void);
GS_S32 GS_SOCK_DeInit(GS_Void);
GS_S32 GS_SOCK_Socket(GS_S32 s32Domain, GS_S32 type, GS_S32 s32Protocol);
GS_S32 GS_SOCK_Bind(GS_S32 s32Sock, const GS_Sockaddr_S *pstAddr, GS_U32 u32AddrLen);
GS_S32 GS_SOCK_Connect(GS_S32 s32Sock, const GS_Sockaddr_S *pstAddr, GS_U32 u32AddrLen);
GS_S32 GS_SOCK_Recv(GS_S32 s32Sock, GS_U8 *pu8Buf, GS_U32 u32Len, GS_S32 s32Flag);
GS_S32 GS_SOCK_Recvfrom(GS_S32 s32Sock, GS_U8 *pu8Buf, GS_U32 u32Len, GS_S32 s32Flag, GS_Sockaddr_S *pstAddr, GS_U32 *pu32AddrLen);
GS_S32 GS_SOCK_SetRecvTimeout(GS_S32 s32Sock, GS_U32 u32TimeMs);
GS_S32 GS_SOCK_Send(GS_S32 s32Sock, GS_U8 *pu8Buf, GS_U32 u32Len, GS_S32 s32Flag);
GS_S32 GS_SOCK_Sendto(GS_S32 s32Sock, GS_U8 *pu8Buf, GS_U32 u32Len, GS_S32 s32Flag, GS_Sockaddr_S *pstAddr, GS_U32 u32AddrLen);
GS_S32 GS_SOCK_Select(GS_S32 *ps32Sock, GS_U32 u32Count, GS_U32 u32TimeMs);
GS_S32 GS_SOCK_Close(GS_S32 s32Sock);
GS_S32 GS_SOCK_GetLastError(GS_Void);

GS_S32 GS_SOCK_StrToIPv4(GS_Char *szStr, GS_U8 *pu8IPv4, GS_U16 *pu16Port);
GS_S32 GS_SOCK_Inet_aton(const GS_Char *szInput, GS_U8 *pu8IPv4);

#ifdef __cplusplus
}
#endif
#endif

