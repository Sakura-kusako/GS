#include "gs_socket.h"
#include "gs_stdlib.h"
#include "gs_net.h"
#include "gs_app_protocol.h"
#include "gs_os.h"
#include "gs_console.h"

typedef struct
{
    GS_S32  s32Sock;                        ///< socket
    GS_U16  u16LocalDebugPort;              ///< 调试器端口
    GS_U16  u16LocalClientPort;             ///< 客户端端口

    GS_U32  u32KeepAliveTimeMs; ///< 最近一次发送心跳包时间
    GS_U32  u32LastRecvTimeMs;  ///< 收到最近一次心跳包应答时间
    GS_U8   au8SendBuf[GS_APP_SEND_BUF_SIZE];   ///< 发送buffer
    GS_U32  u32SendLen;                         ///< 发送长度
    GS_U8   au8RecvBuf[GS_APP_RECV_BUF_SIZE];   ///< 接收buffer
    GS_U32  u32RecvLen;                         ///< 接收长度
} GS_DebugManager_S;

static GS_DebugManager_S s_stManager;

static GS_Void GS_SendToClient(GS_DebugManager_S *pstManager, GS_U8 u8Type, GS_U32 u32Len, GS_U8 *pu8Data)
{
    GS_S32 s32Ret = 0;
    GS_SockaddrIn_S stAddr = {0};

    GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);

    u32Len = (u32Len > 255) ? 255 : u32Len;
    GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen, u8Type, (GS_U8)u32Len, pu8Data);

    GS_Memset(&stAddr, 0, sizeof(GS_SockaddrIn_S));
    stAddr.s16Family = GS_AF_INET;
    stAddr.u16Port = GS_Htons(pstManager->u16LocalClientPort);
    GS_SOCK_SetIPv4Local(stAddr.au8IPv4);

    s32Ret = GS_SOCK_Sendto(pstManager->s32Sock, pstManager->au8SendBuf, pstManager->u32SendLen,
        0, (GS_Sockaddr_S *)&stAddr, sizeof(GS_SockaddrIn_S));
    if (s32Ret != pstManager->u32SendLen)
    {
        GS_ERROR("GS_SOCK_Sendto error");
        GS_OS_SleepMs(1000);
    }

    return ;
}

static GS_S32 GS_APP_CmdOpenClient(GS_DebugManager_S *pstManager)
{
    GS_Snprintf((GS_Char *)pstManager->au8SendBuf, GS_APP_SEND_BUF_SIZE, "start gs_client.exe %u %u",
        pstManager->u16LocalClientPort, pstManager->u16LocalDebugPort);
    GS_OS_System((GS_Char *)pstManager->au8SendBuf);
    GS_INFO("GS_OS_System(%s)", (GS_Char *)pstManager->au8SendBuf);
    return 0;
}

static GS_S32 GS_APP_Init(GS_DebugManager_S *pstManager)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_SockaddrIn_S stAddr = {0};
    GS_Socket s32Sock = -1;
    GS_S32 i = 0;

    do
    {
        s32Sock = GS_SOCK_Socket(GS_AF_INET, GS_SOCK_DGRAM, 0);

        GS_Memset(&stAddr, 0, sizeof(GS_SockaddrIn_S));
        stAddr.s16Family = GS_AF_INET;
        GS_SOCK_SetIPv4Local(stAddr.au8IPv4);

        for (i=0; i<500; i++)
        {
            GS_INFO("socket(%d) bind(0.0.0.0:%u)", i, pstManager->u16LocalDebugPort + i);
            stAddr.u16Port = GS_Htons(pstManager->u16LocalDebugPort + i);
            s32Ret = GS_SOCK_Bind(s32Sock, (GS_Sockaddr_S *)&stAddr, sizeof(GS_Sockaddr_S));
            if (s32Ret < 0)
            {
                continue;
            }

            GS_ERROR("bind debug port success");
            pstManager->s32Sock = s32Sock;
            pstManager->u16LocalDebugPort += i;
            break;
        }

        if (i == 500)
        {
            GS_ERROR("bind debug port error");
            s32Ret = GS_FAILED;
            break;
        }

        GS_INFO("socket set timeout");
        s32Ret = GS_SOCK_SetRecvTimeout(s32Sock, 50);
        if (s32Ret < 0)
        {
            GS_ERROR("GS_SOCK_SetRecvTimeout error");
            break;
        }

        GS_APP_CmdOpenClient(pstManager);

        s32Ret = GS_SUCCESS;
        GS_INFO("app init success");
    }while (0);

    return s32Ret;
}

static GS_Void GS_APP_ManagerInit(GS_DebugManager_S *pstManager)
{
    GS_Memset(pstManager, 0, sizeof(GS_DebugManager_S));
    pstManager->s32Sock = -1;
    pstManager->u16LocalClientPort = 16384;
    pstManager->u16LocalDebugPort = 10000;
}

static GS_Void GS_APP_DebugFrameParse(GS_Void *pvData, GS_U8 u8Type, GS_U32 u32Len, GS_U8 *pu8Data)
{
    GS_S32 s32Ret = 0;
    GS_DebugManager_S *pstManager = (GS_DebugManager_S *)pvData;

    GS_AVOID_WARNING(s32Ret);

    switch (u8Type)
    {
        case GS_APP_IE_TEXT:
        {
            if (u32Len > 1)
            {
                pu8Data[u32Len - 1] = '\0';
                GS_Printf("<note> [%s]\r\n", pu8Data);
            }

            break;
        }

        case GS_APP_IE_DEBUG_HEARTBEAT_RSP:
        {
            pstManager->u32LastRecvTimeMs = GS_OS_GetTimeMs();
            break;
        }

        default:
            break;
    }

    return ;
}

static GS_Void *fnThreadMain(GS_Void *pvArg)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_DebugManager_S *pstManager = (GS_DebugManager_S *)pvArg;
    GS_SockaddrIn_S stAddr = {0};
    GS_S32 s32Sock = 0;
    GS_U32 u32AddrLen = 0;

    GS_INFO("fnThreadMain run");

    do
    {
        s32Ret = GS_APP_Init(pstManager);
        if (s32Ret != GS_SUCCESS)
        {
            GS_ERROR("GS_APP_Init error");
            break;
        }

        pstManager->u32LastRecvTimeMs = GS_OS_GetTimeMs();
        s32Sock = pstManager->s32Sock;

        while (1)
        {
            if (GS_OS_GetTimeMs() - pstManager->u32KeepAliveTimeMs > 2000)
            {
                pstManager->u32KeepAliveTimeMs = GS_OS_GetTimeMs();
                GS_SendToClient(pstManager, GS_APP_IE_DEBUG_HEARTBEAT, 0, GS_NULL);
            }

            u32AddrLen = sizeof(GS_SockaddrIn_S);
            s32Ret = GS_SOCK_Recvfrom(s32Sock, pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE,
                GS_APP_RECV_BUF_SIZE - GS_APP_BUF_HEAD_RESERVE_SIZE, 0, (GS_Sockaddr_S *)&stAddr, &u32AddrLen);
            if (s32Ret < 0)
            {
                s32Ret = GS_SOCK_GetLastError();
                if ((GS_ERRNO_AGAIN == s32Ret) || (GS_ERRNO_TIMEOUT == s32Ret))
                {
                    continue;
                }
                else
                {
                    GS_ERROR("GS_SOCK_Recvfrom error, errno = %d", s32Ret);
                    GS_OS_SleepMs(1000);
                    continue;
                }
            }

            pstManager->u32RecvLen = s32Ret;

            s32Ret = GS_APP_FrameCheck(pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE, pstManager->u32RecvLen);
            if (GS_SUCCESS != s32Ret)
            {
                GS_ERROR("GS_APP_FrameCheck error, len = %u", pstManager->u32RecvLen);
                continue;
            }

            GS_APP_FrameParse(pstManager, pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE,
                pstManager->u32RecvLen, GS_APP_DebugFrameParse);
        }
    }while (0);

    GS_ERROR("app exit");
    GS_SOCK_DeInit();
    GS_OS_SleepMs(5000);
    GS_OS_Exit(0);
    return GS_NULL;
}

static GS_Void *fnThreadInput(GS_Void *pvArg)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_DebugManager_S *pstManager = (GS_DebugManager_S *)pvArg;
    GS_U32 u32Len = 0;
    GS_Char szCmd[1024] = {0};

    GS_INFO("fnThreadInput run");

    do
    {
        while (1)
        {
            u32Len = sizeof(szCmd);
            GS_Memset(szCmd, 0, u32Len);
            s32Ret = GS_CONSOLE_GetInput(szCmd, &u32Len);
            if (GS_SUCCESS != s32Ret)
            {
                GS_OS_SleepMs(500);
                continue;
            }

            if (0 == GS_Memcmp("open ", szCmd, 5))
            {
                pstManager->u16LocalClientPort = GS_Atoi(szCmd + 5);
                GS_APP_CmdOpenClient(pstManager);
            }

            GS_SendToClient(pstManager, GS_APP_IE_DEBUG_CMD, u32Len, (GS_U8 *)szCmd);
        }
    }while (0);

    GS_ERROR("app exit");
    GS_SOCK_DeInit();
    GS_OS_SleepMs(5000);
    GS_OS_Exit(0);
    return GS_NULL;
}

GS_Int main(GS_Void)
{
    GS_DebugManager_S *pstManager = &s_stManager;

    GS_INFO("main start");

    GS_APP_ManagerInit(pstManager);
    GS_SOCK_Init();
    GS_OS_CreateThread(0, fnThreadMain, pstManager);
    GS_OS_CreateThread(0, fnThreadInput, pstManager);
    while (1)
        GS_OS_SleepMs(60000);

    return 0;
}


