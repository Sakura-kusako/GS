#include "gs_socket.h"
#include "gs_net.h"
#include "gs_app_protocol.h"
#include "gs_os.h"
#include "gs_stdlib.h"
#include "gs_console.h"

#define GS_APP_MAX_PLAYER_NUM   (15)
#define GS_APP_PLAYER_ACTIVE_TIMEOUT_MS (10 * 60 * 1000) //10分钟

typedef enum
{
    EN_GS_CLIENT_STATUS_INIT = 0,
    EN_GS_CLIENT_STATUS_EXIT,
    EN_GS_CLIENT_STATUS_CONNECTING,
    EN_GS_CLIENT_STATUS_CONNECTED,
    EN_GS_CLIENT_STATUS_BUTT
} GS_ClientStatus_E;

typedef GS_S32 (*GS_APP_CmdEntry_F)(GS_Void *pvData, GS_S32 argc, GS_Char *argv[]);
typedef struct
{
    GS_Char *szCmd;
    GS_Char *szHelp;
    GS_APP_CmdEntry_F fnEntry;
} GS_ConsoleInfo_S;

typedef struct
{
    GS_Char szPlayerName[GS_APP_PLAYER_NAME_SIZE];  ///< 玩家名
    GS_NetEndPoint_S stPlayerEndPoint;              ///< 玩家外网地址
    GS_U16  u16LocalTransmitProt;                   ///< 本地传输端口
    GS_U32  u32LastActiveTimeMs;                    ///< 最后一次活动时间
} GS_ClientPlayer_S;

typedef struct
{
    GS_U8   au8ServerIPv4[GS_IPV4_LEN];     ///< 服务器IPv4地址
    GS_U16  u16ServerListenPort;            ///< 服务器监听端口
    GS_U16  u16ServerTransmitPort;          ///< 服务器传输端口

    GS_ClientPlayer_S astPlayerInfo[GS_APP_MAX_PLAYER_NUM]; ///< 转发玩家信息
    GS_S32  as32Socket[GS_APP_MAX_PLAYER_NUM];              ///< 本地转发socket
    GS_U16  u16LocalGamePort;                               ///< 本地游戏客户端端口
    GS_U16  u16LocalDebugPort;                              ///< 本地调试器端口

    GS_U8   u8ClientStatus;     ///< 详见<GS_ClientStatus_E>
    GS_U32  u32ConnectTimeMs;   ///< 最近一次发起连接服务器时间
    GS_U32  u32KeepAliveTimeMs; ///< 最近一次发送心跳包时间
    GS_U32  u32LastRecvTimeMs;  ///< 收到最近一次服务器心跳包应答时间
    GS_U32  u32DebugRecvTimeMs; ///< 收到最近一次本地调试窗口心跳包时间
    GS_U8   au8SendBuf[GS_APP_SEND_BUF_SIZE];   ///< 发送buffer
    GS_U32  u32SendLen;                         ///< 发送长度
    GS_U8   au8RecvBuf[GS_APP_RECV_BUF_SIZE];   ///< 接收buffer
    GS_U32  u32RecvLen;                         ///< 接收长度

    GS_ConsoleInfo_S *pstConsoleInfoList;

    GS_Char szPlayerName[GS_APP_PLAYER_NAME_SIZE];   ///< 玩家名
    GS_APP_RoomInfo_S stRoomInfo;
} GS_ClientManager_S;

static GS_S32 GS_APP_CmdDebug(GS_Void *pvData, GS_S32 argc, GS_Char *argv[]);
static GS_S32 GS_APP_CmdSetServerIPv4(GS_Void *pvData, GS_S32 argc, GS_Char *argv[]);

static GS_ClientManager_S s_stManager;
static GS_ConsoleInfo_S s_astConsoleInfo[] =
{
    {"get_cmd_list", "get all supported cmd. <get_cmd_list>", GS_NULL},
    {"debug", "switch the debug console windows. <debug 1>", GS_APP_CmdDebug},
    {"set_server_ip", "set server ipv4 and port. <set_server_ip 127.0.0.1:10800>", GS_APP_CmdSetServerIPv4},
    {GS_NULL, GS_NULL, GS_NULL}
};

static GS_U8 APP_GetEndPointIndex(GS_ClientManager_S *pstManager, GS_U8 *pu8EndPoint)
{
    GS_ClientPlayer_S *pstPlayer = GS_NULL;
    GS_NetEndPoint_S *pstEP = GS_NULL;
    GS_U32 u32TimeMs = 0;
    GS_U8 u8Index = GS_APP_MAX_PLAYER_NUM;

    if (!pstManager || !pu8EndPoint)
    {
        GS_ERROR("bad para");
        return GS_APP_MAX_PLAYER_NUM;
    }

    for (u8Index=1; u8Index<GS_APP_MAX_PLAYER_NUM; u8Index++)
    {
        pstEP = &pstManager->astPlayerInfo[u8Index].stPlayerEndPoint;

        if (0 == pstEP->u16Port)
        {
            GS_Memcpy(pstEP, pu8EndPoint, sizeof(GS_NetEndPoint_S));
            break;
        }

        if (0 == GS_Memcmp(pstEP, pu8EndPoint, sizeof(GS_NetEndPoint_S)))
        {
            break;
        }
    }

    /* 如果没有空位则替换掉很久不活动的玩家 */
    if (u8Index == GS_APP_MAX_PLAYER_NUM)
    {
        u32TimeMs = GS_OS_GetTimeMs();
        for (u8Index=1; u8Index<GS_APP_MAX_PLAYER_NUM; u8Index++)
        {
            pstPlayer = &pstManager->astPlayerInfo[u8Index];
            if (u32TimeMs - pstPlayer->u32LastActiveTimeMs >= GS_APP_PLAYER_ACTIVE_TIMEOUT_MS)
            {
                pstEP = &pstPlayer->stPlayerEndPoint;
                GS_Memcpy(pstEP, pu8EndPoint, sizeof(GS_NetEndPoint_S));
                break;
            }
        }
    }

    return u8Index;
}

static GS_S32 GS_APP_Init(GS_ClientManager_S *pstManager)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_SockaddrIn_S stAddr;
    GS_ClientPlayer_S *pstPlayerInfo = GS_NULL;
    GS_S32 s32Sock = 0;
    GS_S32 i = 0;
    GS_S32 j = 0;
    GS_U16 u16PortBase = 0;

    do
    {
        s32Ret = GS_SOCK_Init();
        if (s32Ret < 0)
        {
            GS_ERROR("GS_SOCK_Init error");
            break;
        }

        u16PortBase = pstManager->astPlayerInfo[0].u16LocalTransmitProt;

        for (i=0; i<GS_APP_MAX_PLAYER_NUM; i++)
        {
            pstPlayerInfo = &pstManager->astPlayerInfo[i];
            pstPlayerInfo->u16LocalTransmitProt = u16PortBase + i;

            s32Sock = GS_SOCK_Socket(GS_AF_INET, GS_SOCK_DGRAM, 0);
            if (s32Sock < 0)
            {
                GS_ERROR("GS_SOCK_Socket error");
                break;
            }

            pstManager->as32Socket[i] = s32Sock;

            GS_Memset(&stAddr, 0, sizeof(GS_SockaddrIn_S));
            stAddr.s16Family = GS_AF_INET;

            /* 尝试5次bind直到成功 */
            for (j=0; j<250; j+=50)
            {
                GS_INFO("socket(%d) bind(0.0.0.0:%u)", i, pstPlayerInfo->u16LocalTransmitProt + j);
                stAddr.u16Port = GS_Htons(pstPlayerInfo->u16LocalTransmitProt + j);
                s32Ret = GS_SOCK_Bind(s32Sock, (GS_Sockaddr_S *)&stAddr, sizeof(GS_Sockaddr_S));
                if (s32Ret < 0)
                {
                    GS_ERROR("GS_SOCK_Bind error");
                    if (i == 0)
                    {
                        /* 与服务器连接的端口bind失败立即退出 */
                        j = 250;
                        break;
                    }
                }
                else
                {
                    pstPlayerInfo->u16LocalTransmitProt += j;
                    break;
                }
            }

            if (j == 250)
            {
                break;
            }

            if (i > 0)
            {
                GS_INFO("socket(%d) connect(127.0.0.1:%u)", i, pstManager->u16LocalGamePort);
                GS_Memset(&stAddr, 0, sizeof(GS_SockaddrIn_S));
                stAddr.s16Family = GS_AF_INET;
                stAddr.u16Port = GS_Htons(pstManager->u16LocalGamePort);
                GS_SOCK_SetIPv4Local(stAddr.au8IPv4);

                s32Ret = GS_SOCK_Connect(s32Sock, (GS_Sockaddr_S *)&stAddr, sizeof(GS_Sockaddr_S));
                if (s32Ret < 0)
                {
                    GS_ERROR("GS_SOCK_Connect error");
                    break;
                }
            }

            GS_INFO("socket(%d) set timeout", i);
            s32Ret = GS_SOCK_SetRecvTimeout(s32Sock, 50);
            if (s32Ret < 0)
            {
                GS_ERROR("GS_SOCK_SetRecvTimeout error");
                break;
            }
        }

        if (i != GS_APP_MAX_PLAYER_NUM)
        {
            s32Ret = GS_FAILED;
            break;
        }

        pstManager->u32DebugRecvTimeMs = GS_OS_GetTimeMs();

        s32Ret = GS_SUCCESS;
        GS_INFO("app init success");
    }while (0);

    if (s32Ret != GS_SUCCESS)
    {
        for (i=0; i<GS_APP_MAX_PLAYER_NUM; i++)
        {
            if (pstManager->as32Socket[i] >= 0)
            {
                GS_SOCK_Close(pstManager->as32Socket[i]);
            }
        }
    }

    return s32Ret;
}

static GS_Void GS_APP_DeInit(GS_ClientManager_S *pstManager)
{
    GS_S32 i = 0;

    for (i=0; i<GS_APP_MAX_PLAYER_NUM; i++)
    {
        if (pstManager->as32Socket[i] >= 0)
        {
            GS_SOCK_Close(pstManager->as32Socket[i]);
        }
    }
}

static GS_S32 GS_APP_SendTextWithLen(GS_ClientManager_S *pstManager, GS_Char *szStr, GS_U32 u32Len)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_SockaddrIn_S stAddr = {0};

    if (!pstManager || !szStr || u32Len < 2)
    {
        GS_ERROR("bad para");
        return GS_FAILED;
    }

    GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);
    GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
        GS_APP_IE_TEXT, u32Len, (GS_U8 *)szStr);

    stAddr.s16Family = GS_AF_INET;
    stAddr.u16Port = GS_Htons(pstManager->u16LocalDebugPort);
    GS_SOCK_SetIPv4Local(stAddr.au8IPv4);
    s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[0], pstManager->au8SendBuf,
        pstManager->u32SendLen, 0, (GS_Sockaddr_S *)&stAddr, sizeof(GS_SockaddrIn_S));
    if (s32Ret != pstManager->u32SendLen)
    {
        GS_ERROR("GS_SOCK_Sendto error");
        s32Ret = GS_FAILED;
    }

    return s32Ret;
}

static GS_S32 GS_APP_SendText(GS_ClientManager_S *pstManager, GS_Char *szStr)
{
    if (!pstManager || !szStr)
    {
        GS_ERROR("bad para");
        return GS_FAILED;
    }

    return GS_APP_SendTextWithLen(pstManager, szStr, GS_Strlen(szStr) + 1);
}

static GS_S32 GS_APP_CmdDebug(GS_Void *pvData, GS_S32 argc, GS_Char *argv[])
{
    GS_S32 s32Ret = GS_CONSOLE_ERRNO_SUCCESS;
    GS_ClientManager_S *pstManager = (GS_ClientManager_S *)pvData;

    do
    {
        if ((1 != argc) || !argv || !pstManager)
        {
            GS_ERROR("bad para");
            s32Ret = GS_CONSOLE_ERRNO_BAD_PARAMETER;
            break;
        }

        GS_OS_Window_Switch(argv[1][0] == '1');
        GS_INFO("switch windows(%u) success!", (argv[1][0] == '1'));
        GS_APP_SendText(pstManager, "switch windows success!");
    }while (0);

    return s32Ret;
}

static GS_S32 GS_APP_CmdSetServerIPv4(GS_Void *pvData, GS_S32 argc, GS_Char *argv[])
{
    GS_S32 s32Ret = GS_CONSOLE_ERRNO_SUCCESS;
    GS_ClientManager_S *pstManager = (GS_ClientManager_S *)pvData;

    do
    {
        if ((1 != argc) || !argv || !pstManager)
        {
            GS_ERROR("bad para");
            s32Ret = GS_CONSOLE_ERRNO_BAD_PARAMETER;
            break;
        }

        s32Ret = GS_SOCK_StrToIPv4(argv[1], pstManager->au8ServerIPv4, &pstManager->u16ServerListenPort);
        if (GS_SUCCESS != s32Ret)
        {
            GS_ERROR("GS_SOCK_StrToIPv4 error");
            s32Ret = GS_CONSOLE_ERRNO_BAD_PARAMETER;
            break;
        }

        GS_INFO("set server ip:port "GS_IPV4_FMT":%u success!",
            GS_IPV4_ARG(pstManager->au8ServerIPv4), pstManager->u16ServerListenPort);
        GS_APP_SendText(pstManager, "set server ip:port success");
    }while (0);

    return s32Ret;
}

static GS_Void GS_APP_DefaultFrameParse(GS_Void *pvData, GS_U8 u8Type, GS_U32 u32Len, GS_U8 *pu8Data)
{
    GS_S32 s32Ret = 0;
    GS_ClientManager_S *pstManager = (GS_ClientManager_S *)pvData;
    GS_SockaddrIn_S stAddr = {0};
    GS_ConsoleInfo_S *pstConsloeInfo = GS_NULL;
    GS_Char *s32Argv[4] = {GS_NULL};
    GS_S32 s32Argc = sizeof(s32Argv) / sizeof(s32Argv[0]);

    switch (u8Type)
    {
        case GS_APP_IE_TEXT:
        {
            if (u32Len > 1)
            {
                pu8Data[u32Len - 1] = '\0';
                GS_INFO("<note> [%s]", pu8Data);
            }

            break;
        }

        case GS_APP_IE_DEBUG_HEARTBEAT:
        {
            pstManager->u32DebugRecvTimeMs = GS_OS_GetTimeMs();

            GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);
            GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                GS_APP_IE_DEBUG_HEARTBEAT_RSP, 0, GS_NULL);

            stAddr.s16Family = GS_AF_INET;
            stAddr.u16Port = GS_Htons(pstManager->u16LocalDebugPort);
            GS_SOCK_SetIPv4Local(stAddr.au8IPv4);
            s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[0], pstManager->au8SendBuf,
                pstManager->u32SendLen, 0, (GS_Sockaddr_S *)&stAddr, sizeof(GS_SockaddrIn_S));
            if (s32Ret != pstManager->u32SendLen)
            {
                GS_ERROR("GS_SOCK_Sendto error");
            }

            break;
        }

        case GS_APP_IE_DEBUG_CMD:
        {
            s32Ret = GS_CONSOLE_ParseCmd((GS_Char *)pu8Data, &s32Argc, s32Argv);
            if (GS_SUCCESS != s32Ret)
            {
                GS_ERROR("GS_CONSOLE_ParseCmd error");
            }

            pstConsloeInfo = pstManager->pstConsoleInfoList;
            while (pstConsloeInfo->szCmd)
            {
                if (0 == GS_Strcmp(pstConsloeInfo->szCmd, s32Argv[0]))
                {
                    if (pstConsloeInfo->fnEntry)
                    {
                        pstConsloeInfo->fnEntry(pstManager, s32Argc - 1, s32Argv);
                    }

                    break;
                }

                pstConsloeInfo++;
            }

            if (!pstConsloeInfo->szCmd)
            {
                GS_ERROR("unknown cmd [%s]", pu8Data);
                GS_APP_SendText(pstManager, "unknown cmd");
            }

            break;
        }

        default:
            break;
    }

    return ;
}

static GS_Void GS_APP_StatusConnectingFrameParse(GS_Void *pvData, GS_U8 u8Type, GS_U32 u32Len, GS_U8 *pu8Data)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_ClientManager_S *pstManager = (GS_ClientManager_S *)pvData;
    GS_SockaddrIn_S stAddr;
    GS_Char szStr[128] = {0};

    GS_AVOID_WARNING(s32Ret);
    GS_AVOID_WARNING(stAddr);

    GS_APP_DefaultFrameParse(pvData, u8Type, u32Len, pu8Data);

    switch (u8Type)
    {
        case GS_APP_IE_CONNECT_RSP:
        {
            pstManager->u16ServerTransmitPort = GS_APP_GET_U16(pu8Data);

            #if 0
            GS_Memset(&stAddr, 0, sizeof(GS_SockaddrIn_S));
            stAddr.s16Family = GS_AF_INET;
            stAddr.u16Port = GS_Htons(pstManager->u16ServerTransmitPort);
            GS_IPV4_SET(stAddr.au8IPv4, pstManager->au8ServerIPv4);

            s32Ret = GS_SOCK_Connect(pstManager->as32Socket[0], (GS_Sockaddr_S *)&stAddr, sizeof(GS_Sockaddr_S));
            if (s32Ret < 0)
            {
                GS_ERROR("GS_SOCK_Connect error");
                pstManager->u8ClientStatus = EN_GS_CLIENT_STATUS_EXIT;
                break;
            }
            #endif

            pstManager->u32LastRecvTimeMs = GS_OS_GetTimeMs();
            pstManager->u32KeepAliveTimeMs = GS_OS_GetTimeMs();
            pstManager->u8ClientStatus = EN_GS_CLIENT_STATUS_CONNECTED;
            GS_INFO("connecting --> connected, server "GS_IPV4_FMT":%u",
                GS_IPV4_ARG(pstManager->au8ServerIPv4), pstManager->u16ServerTransmitPort);
            GS_Snprintf(szStr, sizeof(szStr), GS_IPV4_FMT":%u",
                GS_IPV4_ARG(pstManager->au8ServerIPv4), pstManager->u16ServerTransmitPort);
            GS_APP_SendText(pstManager, szStr);
            break;
        }

        default:
        {
            //GS_INFO("unknown type(%u) len(%u)", u8Type, u32Len);
            break;
        }
    }

    return ;
}

static GS_S32 GS_APP_StatusConnecting(GS_ClientManager_S *pstManager)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_SockaddrIn_S stAddr = {0};
    GS_U32 u32AddrLen = 0;
    GS_S32 s32Sock = 0;

    do
    {
        if (!pstManager)
        {
            GS_ERROR("bad para");
            s32Ret = GS_FAILED;
            break;
        }

        s32Sock = pstManager->as32Socket[0];

        if (GS_OS_GetTimeMs() - pstManager->u32ConnectTimeMs > 1000)
        {
            pstManager->u32ConnectTimeMs = GS_OS_GetTimeMs();
            GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);
            GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                GS_APP_IE_CONNECT_REQ, 0, GS_NULL);

            GS_INFO("send connect req");
            GS_Memset(&stAddr, 0, sizeof(GS_SockaddrIn_S));
            stAddr.s16Family = GS_AF_INET;
            stAddr.u16Port = GS_Htons(pstManager->u16ServerListenPort);
            GS_Memcpy(stAddr.au8IPv4, pstManager->au8ServerIPv4, GS_IPV4_LEN);
            s32Ret = GS_SOCK_Sendto(s32Sock, pstManager->au8SendBuf, pstManager->u32SendLen,
                0, (GS_Sockaddr_S *)&stAddr, sizeof(GS_SockaddrIn_S));
            if (s32Ret != pstManager->u32SendLen)
            {
                GS_ERROR("GS_SOCK_Sendto error");
                s32Ret = GS_FAILED;
                break;
            }
        }

        u32AddrLen = sizeof(GS_SockaddrIn_S);
        s32Ret = GS_SOCK_Recvfrom(s32Sock, pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE,
            GS_APP_RECV_BUF_SIZE - GS_APP_BUF_HEAD_RESERVE_SIZE, 0, (GS_Sockaddr_S *)&stAddr, &u32AddrLen);
        if (s32Ret < 0)
        {
            s32Ret = GS_SOCK_GetLastError();
            if ((GS_ERRNO_AGAIN == s32Ret) || (GS_ERRNO_TIMEOUT == s32Ret))
            {
                s32Ret = GS_SUCCESS;
                break;
            }
            else
            {
                GS_ERROR("GS_SOCK_Recvfrom error, errno = %d", s32Ret);
                s32Ret = GS_FAILED;
                break;
            }
        }

        pstManager->u32RecvLen = s32Ret;

        s32Ret = GS_APP_FrameCheck(pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE, pstManager->u32RecvLen);
        if (GS_SUCCESS != s32Ret)
        {
            GS_ERROR("GS_APP_FrameCheck error, len = %u", pstManager->u32RecvLen);
            s32Ret = GS_SUCCESS;
            break;
        }

        GS_APP_FrameParse(pstManager, pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE,
            pstManager->u32RecvLen, GS_APP_StatusConnectingFrameParse);
    }while (0);

    if (GS_FAILED == s32Ret)
    {
        pstManager->u8ClientStatus = EN_GS_CLIENT_STATUS_EXIT;
    }

    return s32Ret;
}

static GS_Void GS_APP_StatusConnectedFrameParse(GS_Void *pvData, GS_U8 u8Type, GS_U32 u32Len, GS_U8 *pu8Data)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_ClientManager_S *pstManager = (GS_ClientManager_S *)pvData;

    GS_APP_DefaultFrameParse(pvData, u8Type, u32Len, pu8Data);

    switch (u8Type)
    {
        case GS_APP_IE_THANSMIT:
        {
            u32Len -= sizeof(GS_NetEndPoint_S);
            s32Ret = APP_GetEndPointIndex(pstManager, pu8Data + u32Len);
            if (s32Ret >= GS_APP_MAX_PLAYER_NUM)
            {
                GS_INFO("player full");
                break;
            }

            pstManager->astPlayerInfo[s32Ret].u32LastActiveTimeMs = GS_OS_GetTimeMs();

            s32Ret = GS_SOCK_Send(pstManager->as32Socket[s32Ret], pu8Data, u32Len, 0);
            if (s32Ret != u32Len)
            {
                GS_ERROR("GS_SOCK_Send error");
            }

            break;
        }

        case GS_APP_IE_HEARTBEAT_RSP:
        {
            //GS_INFO("recv heartbeat rsp");
            pstManager->u32LastRecvTimeMs = GS_OS_GetTimeMs();
            break;
        }

        default:
        {
            //GS_INFO("unknown type(%u) len(%u)", u8Type, u32Len);
            break;
        }
    }

    return ;
}

static GS_S32 GS_APP_StatusConnected(GS_ClientManager_S *pstManager)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_S32 s32Sock = 0;
    GS_S32 s32Index = 0;
    GS_SockaddrIn_S stAddr = {0};
    GS_U32 u32AddrLen = 0;

    do
    {
        if (!pstManager)
        {
            GS_ERROR("bad para");
            s32Ret = GS_FAILED;
            break;
        }

        s32Sock = pstManager->as32Socket[0];
        if (GS_OS_GetTimeMs() - pstManager->u32KeepAliveTimeMs > 2000)
        {
            pstManager->u32KeepAliveTimeMs = GS_OS_GetTimeMs();
            GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);
            GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                GS_APP_IE_HEARTBEAT, 0, GS_NULL);

            stAddr.s16Family = GS_AF_INET;
            stAddr.u16Port = GS_Htons(pstManager->u16ServerTransmitPort);
            GS_IPV4_SET(stAddr.au8IPv4, pstManager->au8ServerIPv4);

            s32Ret = GS_SOCK_Sendto(s32Sock, pstManager->au8SendBuf, pstManager->u32SendLen, 0,
                (GS_Sockaddr_S *)&stAddr, sizeof(GS_SockaddrIn_S));
            if (s32Ret != pstManager->u32SendLen)
            {
                GS_ERROR("GS_SOCK_Send error");
                s32Ret = GS_FAILED;
                break;
            }
        }

        if (GS_OS_GetTimeMs() - pstManager->u32LastRecvTimeMs > 30000)
        {
            GS_ERROR("keep alive timeout");
            s32Ret = GS_FAILED;
            break;
        }

        s32Ret = GS_SOCK_Select(pstManager->as32Socket, GS_APP_MAX_PLAYER_NUM, 50);
        if (s32Ret == -1)
        {
            GS_ERROR("GS_SOCK_Select error");
            s32Ret = GS_FAILED;
            break;
        }
        else if (s32Ret == -2)
        {
            s32Ret = GS_SUCCESS;
            break;
        }

        s32Index = s32Ret;
        s32Sock = pstManager->as32Socket[s32Index];

        if (0 == s32Index)
        {
            u32AddrLen = sizeof(GS_SockaddrIn_S);
            s32Ret = GS_SOCK_Recvfrom(s32Sock, pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE,
                GS_APP_RECV_BUF_SIZE - GS_APP_BUF_HEAD_RESERVE_SIZE, 0, (GS_Sockaddr_S *)&stAddr, &u32AddrLen);
            if (s32Ret < 0)
            {
                GS_ERROR("GS_SOCK_Recv error");
                s32Ret = GS_FAILED;
                break;
            }

            /* 只接受服务器和本机的帧 */
            if ((!GS_IPV4_CMP(stAddr.au8IPv4, pstManager->au8ServerIPv4)
                    || (stAddr.u16Port != GS_Htons(pstManager->u16ServerTransmitPort)))
                 && (stAddr.au8IPv4[0] != 127))
            {
                GS_ERROR("recv unknown addr ["GS_IPV4_FMT":%u], len = %d",
                    s32Ret, GS_IPV4_ARG(stAddr.au8IPv4), GS_Ntohs(stAddr.u16Port));
                s32Ret = GS_SUCCESS;
                break;
            }

            pstManager->u32RecvLen = s32Ret;

            s32Ret = GS_APP_FrameCheck(pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE, pstManager->u32RecvLen);
            if (GS_SUCCESS != s32Ret)
            {
                GS_ERROR("GS_APP_FrameCheck error, len = %u", pstManager->u32RecvLen);
                s32Ret = GS_SUCCESS;
                break;
            }

            GS_APP_FrameParse(pstManager, pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE,
                pstManager->u32RecvLen, GS_APP_StatusConnectedFrameParse);
        }
        else
        {
            s32Ret = GS_SOCK_Recv(s32Sock, pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE,
                GS_APP_RECV_BUF_SIZE - GS_APP_BUF_HEAD_RESERVE_SIZE, 0);
            if (s32Ret < 0)
            {
                GS_ERROR("GS_SOCK_Recv error");
                s32Ret = GS_FAILED;
                break;
            }

            pstManager->u32RecvLen = s32Ret;

            if (pstManager->astPlayerInfo[s32Index].stPlayerEndPoint.u16Port == 0)
            {
                GS_ERROR("Wait connected, skip send");
                s32Ret = GS_SUCCESS;
                break;
            }

            s32Ret = GS_APP_FrameToTransmit(pstManager->au8RecvBuf, &pstManager->u32RecvLen,
                &pstManager->astPlayerInfo[s32Index].stPlayerEndPoint);
            if (s32Ret < 0)
            {
                GS_ERROR("GS_APP_FrameToTransmit error");
                s32Ret = GS_FAILED;
                break;
            }

            stAddr.s16Family = GS_AF_INET;
            stAddr.u16Port = GS_Htons(pstManager->u16ServerTransmitPort);
            GS_IPV4_SET(stAddr.au8IPv4, pstManager->au8ServerIPv4);

            s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[0], pstManager->au8RecvBuf + s32Ret,
                pstManager->u32RecvLen, 0, (GS_Sockaddr_S *)&stAddr, sizeof(GS_SockaddrIn_S));
            if (s32Ret != pstManager->u32RecvLen)
            {
                GS_ERROR("GS_SOCK_Sendto error");
                s32Ret = GS_FAILED;
                break;
            }

            pstManager->astPlayerInfo[s32Index].u32LastActiveTimeMs = GS_OS_GetTimeMs();
        }
    }while (0);

    if (GS_FAILED == s32Ret)
    {
        pstManager->u8ClientStatus = EN_GS_CLIENT_STATUS_EXIT;
    }

    return s32Ret;
}

static GS_Void *fnThreadMain(GS_Void *pvArg)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_ClientManager_S *pstManager = (GS_ClientManager_S *)pvArg;

    do
    {
        pstManager->u8ClientStatus = EN_GS_CLIENT_STATUS_INIT;

        s32Ret = GS_APP_Init(pstManager);
        if (s32Ret != GS_SUCCESS)
        {
            GS_ERROR("GS_APP_Init error");
            break;
        }

        pstManager->u8ClientStatus = EN_GS_CLIENT_STATUS_CONNECTING;

        while (1)
        {
            if (EN_GS_CLIENT_STATUS_EXIT == pstManager->u8ClientStatus)
            {
                GS_ERROR("change to exit status");
                break;
            }
            else if (EN_GS_CLIENT_STATUS_CONNECTING == pstManager->u8ClientStatus)
            {
                GS_APP_StatusConnecting(pstManager);
            }
            else if (EN_GS_CLIENT_STATUS_CONNECTED == pstManager->u8ClientStatus)
            {
                GS_APP_StatusConnected(pstManager);
            }
            else
            {
                break;
            }

            if (GS_OS_GetTimeMs() - pstManager->u32DebugRecvTimeMs > 10000)
            {
                GS_ERROR("local heart beat loss");
                break;
            }
        }
    }while (0);

    GS_ERROR("app exit");
    return GS_NULL;
}

static GS_Void GS_APP_ManagerInit(GS_ClientManager_S *pstManager, GS_ConsoleInfo_S *pstInfo)
{
    GS_Memset(pstManager, 0, sizeof(GS_ClientManager_S));
    pstManager->pstConsoleInfoList = pstInfo;
    GS_Memset(pstManager->as32Socket, 0xFF, sizeof(pstManager->as32Socket));
    GS_SOCK_SetIPv4(pstManager->au8ServerIPv4, 106, 13, 239, 194);
    pstManager->u16ServerListenPort = 32768;
    pstManager->u16LocalGamePort = 10800;
}

GS_Int main(GS_Int argc, GS_Char *argv[])
{
    GS_ClientManager_S *pstManager = &s_stManager;

    if (argc != 3)
    {
        return -1;
    }

    GS_APP_ManagerInit(pstManager, s_astConsoleInfo);
    pstManager->astPlayerInfo[0].u16LocalTransmitProt = GS_Atoi(argv[1]);
    pstManager->u16LocalDebugPort = GS_Atoi(argv[2]);
    GS_INFO("transmit port = %u, debug port = %u",
        pstManager->astPlayerInfo[0].u16LocalTransmitProt, pstManager->u16LocalDebugPort);

    GS_OS_Window_Switch(0);
    fnThreadMain(pstManager);
    GS_APP_DeInit(pstManager);
    GS_SOCK_DeInit();
    GS_OS_SleepMs(5000);

    return 0;
}

