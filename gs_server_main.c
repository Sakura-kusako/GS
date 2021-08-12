#include "gs_socket.h"
#include "gs_net.h"
#include "gs_app_protocol.h"
#include "gs_os.h"
#include "gs_stdlib.h"

#define GS_APP_MAX_ROOM_NUM     (63 + 1)
#define GS_APP_MAX_CLIENT_NUM   (63 + 1)
#define GS_SERVER_LOSSLINK_TIMEOUT_MS   (60000)     ///< 服务器断连超时

typedef enum
{
    EN_GS_SERVER_STATUS_INIT = 0,
    EN_GS_SERVER_STATUS_EXIT,
    EN_GS_SERVER_STATUS_LISTEN,
    EN_GS_SERVER_STATUS_BUTT
} GS_ServerStatus_E;

typedef struct
{
    GS_Char szPlayerName[GS_APP_PLAYER_NAME_SIZE];   ///< 玩家名
    GS_U8   au8ClientIPv4[GS_IPV4_LEN]; ///< 客户端的外网IPv4地址
    GS_U16  u16ClientPort;  ///< 客户端的外网端口
    GS_U16  u16ServerPort;  ///< 服务器分配给客户端的外网端口
    GS_U32  u32KeepaliveTimeMs;  ///< 保活时间
    GS_U16  u16RoomIndex;   ///< 所在房间
    GS_U8   au8Zero[2];
} GS_ClientInfo_S;

typedef struct
{
    GS_U16  u16ServerListenPort;    ///< 服务器监听端口
    GS_S32  as32Socket[GS_APP_MAX_CLIENT_NUM];  ///< 本地转发socket
    GS_ClientInfo_S astClientInfo[GS_APP_MAX_CLIENT_NUM];   ///< 客户端信息

    GS_SockaddrIn_S stRecvAddr;                 ///< 接收来源地址
    GS_U32  u32RecvIndex;                       ///< 接收client下标
    GS_U8   au8RecvBuf[GS_APP_RECV_BUF_SIZE];   ///< 接收buffer
    GS_U32  u32RecvLen;                         ///< 接收长度
    GS_U8   au8SendBuf[GS_APP_SEND_BUF_SIZE];   ///< 发送buffer
    GS_U32  u32SendLen;                         ///< 发送长度

    GS_APP_RoomInfo_S astRoomInfo[GS_APP_MAX_ROOM_NUM];

    GS_U8   u8ServerStatus;     ///< 详见<GS_ServerStatus_E>
} GS_ServerManager_S;

static GS_ServerManager_S s_stManager;

static GS_U16 APP_GetClientInfoIndex(GS_ServerManager_S *pstManager)
{
    GS_ClientInfo_S *pstInfo = GS_NULL;
    GS_U8 *pu8IPv4 = GS_NULL;
    GS_U32 u32TimeMs = 0;
    GS_U16 u16Index = GS_APP_MAX_CLIENT_NUM;
    GS_U16 u16Port = 0;

    do
    {
        if (!pstManager)
        {
            GS_ERROR("bad para");
            break;
        }

        pu8IPv4 = pstManager->stRecvAddr.au8IPv4;
        u16Port = pstManager->stRecvAddr.u16Port;

        /* 查找是否在列表中 */
        for (u16Index=1; u16Index<GS_APP_MAX_CLIENT_NUM; u16Index++)
        {
            pstInfo = &pstManager->astClientInfo[u16Index];

            /* 如果不在列表中则添加 */
            if (0 == pstInfo->u16ClientPort)
            {
                GS_IPV4_SET(pstInfo->au8ClientIPv4, pu8IPv4);
                pstInfo->u16ClientPort = u16Port;
                break;
            }

            /* 判断地址是否匹配 */
            if (GS_IPV4_CMP(pstInfo->au8ClientIPv4, pu8IPv4)
                 && (pstInfo->u16ClientPort == u16Port))
            {
                break;
            }
        }

        /* 判断是否已经在列表中 */
        if (u16Index < GS_APP_MAX_CLIENT_NUM)
        {
            break;
        }

        /* 列表满了就看看是否有掉线的客户端 */
        u32TimeMs = GS_OS_GetTimeMs();
        for (u16Index=1; u16Index<GS_APP_MAX_CLIENT_NUM; u16Index++)
        {
            pstInfo = &pstManager->astClientInfo[u16Index];

            /* 替换掉超时的客户端 */
            if (pstInfo->u32KeepaliveTimeMs - u32TimeMs >= GS_SERVER_LOSSLINK_TIMEOUT_MS)
            {
                GS_IPV4_SET(pstInfo->au8ClientIPv4, pu8IPv4);
                pstInfo->u16ClientPort = u16Port;
                break;
            }
        }
    }while (0);

    return u16Index;
}

static GS_S32 GS_APP_Init(GS_ServerManager_S *pstManager)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_ClientInfo_S *pstInfo = GS_NULL;
    GS_SockaddrIn_S stAddr;
    GS_S32 s32Sock = 0;
    GS_S32 i = 0;

    do
    {
        GS_Memset(pstManager, 0, sizeof(GS_ServerManager_S));
        GS_Memset(pstManager->as32Socket, 0xFF, sizeof(pstManager->as32Socket));
        pstManager->u16ServerListenPort = 32768;

        s32Ret = GS_SOCK_Init();
        if (s32Ret < 0)
        {
            GS_ERROR("GS_SOCK_Init error");
            break;
        }

        s32Sock = GS_SOCK_Socket(GS_AF_INET, GS_SOCK_DGRAM, 0);
        if (s32Sock < 0)
        {
            GS_ERROR("GS_SOCK_Socket error");
            break;
        }

        pstManager->as32Socket[0] = s32Sock;

        GS_INFO("socket(%d) bind(0.0.0.0:%u)", 0, pstManager->u16ServerListenPort);
        GS_Memset(&stAddr, 0, sizeof(GS_SockaddrIn_S));
        stAddr.s16Family = GS_AF_INET;
        stAddr.u16Port = GS_Htons(pstManager->u16ServerListenPort);

        s32Ret = GS_SOCK_Bind(s32Sock, (GS_Sockaddr_S *)&stAddr, sizeof(GS_Sockaddr_S));
        if (s32Ret < 0)
        {
            GS_ERROR("GS_SOCK_Bind error");
            break;
        }

        for (i=1; i<GS_APP_MAX_CLIENT_NUM; i++)
        {
            s32Sock = GS_SOCK_Socket(GS_AF_INET, GS_SOCK_DGRAM, 0);
            if (s32Sock < 0)
            {
                GS_ERROR("GS_SOCK_Socket error");
                break;
            }

            pstManager->as32Socket[i] = s32Sock;

            pstInfo = &pstManager->astClientInfo[i];
            pstInfo->u16ServerPort = pstManager->u16ServerListenPort + i;

            GS_INFO("socket(%d) bind(0.0.0.0:%u)", 0, pstInfo->u16ServerPort);
            GS_Memset(&stAddr, 0, sizeof(GS_SockaddrIn_S));
            stAddr.s16Family = GS_AF_INET;
            stAddr.u16Port = GS_Htons(pstInfo->u16ServerPort);

            s32Ret = GS_SOCK_Bind(s32Sock, (GS_Sockaddr_S *)&stAddr, sizeof(GS_Sockaddr_S));
            if (s32Ret < 0)
            {
                GS_ERROR("GS_SOCK_Bind error");
                break;
            }
        }

        s32Ret = GS_SUCCESS;
        GS_INFO("app init success");
    }while (0);

    if (s32Ret != GS_SUCCESS)
    {
        if (pstManager->as32Socket[i] >= 0)
        {
            GS_SOCK_Close(pstManager->as32Socket[0]);
        }
    }

    return s32Ret;
}

static GS_Void GS_APP_StatusListenFrameParse(GS_Void *pvData, GS_U8 u8Type, GS_U32 u32Len, GS_U8 *pu8Data)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_ServerManager_S *pstManager = (GS_ServerManager_S *)pvData;
    GS_ClientInfo_S *pstInfo = &pstManager->astClientInfo[pstManager->u32RecvIndex];
    GS_APP_RoomInfo_S *pstRoom = GS_NULL;
    GS_APP_PlayerInfo_S *pstPlayer = GS_NULL;
    GS_NetEndPoint_S stEP = {0};
    GS_SockaddrIn_S stAddr = {0};
    GS_U32 u32Tmp = 0;
    GS_U32 i = 0;
    GS_U8 u8IEInfo = 0;

    switch (u8Type)
    {
        case GS_APP_IE_THANSMIT:
        {
            s32Ret = GS_APP_FrameGetEndPoint(pu8Data, &u32Len, &stEP);
            if (GS_SUCCESS != s32Ret)
            {
                GS_ERROR("GS_APP_FrameGetEndPoint error");
                break;
            }

            stAddr.s16Family = GS_AF_INET;
            GS_IPV4_SET(stAddr.au8IPv4, stEP.au8IPv4);
            stAddr.u16Port = stEP.u16Port;
            s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[pstManager->u32RecvIndex], pu8Data,
                u32Len, 0, (GS_Sockaddr_S *)&stAddr, sizeof(GS_SockaddrIn_S));
            if (s32Ret != u32Len)
            {
                GS_ERROR("GS_SOCK_Sendto error");
            }

            break;
        }

        case GS_APP_IE_HEARTBEAT:
        {
            pstInfo->u32KeepaliveTimeMs = GS_OS_GetTimeMs();

            GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);
            GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                GS_APP_IE_HEARTBEAT_RSP, 0, GS_NULL);

            //GS_INFO("send heartbeat rsp to "GS_IPV4_FMT":%u",
            //    GS_IPV4_ARG(pstManager->stRecvAddr.au8IPv4), GS_Ntohs(pstManager->stRecvAddr.u16Port));

            s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[pstManager->u32RecvIndex], pstManager->au8SendBuf,
                pstManager->u32SendLen, 0, (GS_Sockaddr_S *)&pstManager->stRecvAddr, sizeof(GS_SockaddrIn_S));
            if (s32Ret != pstManager->u32SendLen)
            {
                GS_ERROR("GS_SOCK_Sendto error");
            }

            break;
        }

        case GS_APP_IE_ROOM_CREATE:
        {
            GS_INFO("unknown type(%u) len(%u)", u8Type, u32Len);

            if (pstInfo->u16RoomIndex > 0)
            {
                GS_INFO("[%s] room create --> send room info", pstInfo->szPlayerName);

                GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);

                u8IEInfo = GS_APP_IE_INFO_ALREADY_IN_ROOM;
                GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                    GS_APP_IE_ROOM_CREATE_RSP, 1, &u8IEInfo);
                GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                    GS_APP_IE_ROOM_INFO, sizeof(GS_APP_RoomInfo_S), &pstManager->astRoomInfo[pstInfo->u16RoomIndex]);

                s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[pstManager->u32RecvIndex], pstManager->au8SendBuf,
                    pstManager->u32SendLen, 0, (GS_Sockaddr_S *)&pstManager->stRecvAddr, sizeof(GS_SockaddrIn_S));
                if (s32Ret != pstManager->u32SendLen)
                {
                    GS_ERROR("GS_SOCK_Sendto error");
                }
            }
            else
            {
                GS_INFO("[%s] room create --> send room create rsp", pstInfo->szPlayerName);

                /* 尝试创建房间 */
                for (i=1; i<GS_APP_MAX_ROOM_NUM; i++)
                {
                    pstRoom = &pstManager->astRoomInfo[i];
                    if (0 == pstRoom->u8IsUsed)
                    {
                        pstRoom->u8IsUsed = 1;
                        pstRoom->u8MasterID = 0;
                        pstRoom->u16RoomID = i;
                        pstRoom->u16RoomIndex = i;
                        if (u32Len >= GS_APP_ROOM_TEXT_SIZE)
                            u32Len = GS_APP_ROOM_TEXT_SIZE - 1;
                        GS_Memcpy(pstRoom->au8Text, pu8Data, u32Len);
                        pstRoom->au8Text[u32Len] = '\0';

                        pstInfo->u16RoomIndex = i;

                        pstPlayer = &pstRoom->astPlayerInfo[0];
                        pstPlayer->u8IsUsed = 1;
                        GS_Memcpy(pstPlayer->szName, pstInfo->szPlayerName, GS_APP_PLAYER_NAME_SIZE + 8);
                        pstPlayer->u16ClientIndex = pstManager->u32RecvIndex;

                        break;
                    }
                }

                if (i < GS_APP_MAX_ROOM_NUM)
                {
                    /* 创建房间成功 */

                    GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);

                    u8IEInfo = GS_APP_IE_INFO_CREATE_NEW_ROOM;
                    GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                        GS_APP_IE_ROOM_CREATE_RSP, 1, &u8IEInfo);
                    GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                        GS_APP_IE_ROOM_INFO, sizeof(GS_APP_RoomInfo_S), &pstRoom);

                    s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[pstManager->u32RecvIndex], pstManager->au8SendBuf,
                        pstManager->u32SendLen, 0, (GS_Sockaddr_S *)&pstManager->stRecvAddr, sizeof(GS_SockaddrIn_S));
                    if (s32Ret != pstManager->u32SendLen)
                    {
                        GS_ERROR("GS_SOCK_Sendto error");
                    }
                }
                else
                {
                    /* 创建房间失败 */

                    GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);

                    u8IEInfo = GS_APP_IE_INFO_NOT_EMPTY_ROOM;
                    GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                        GS_APP_IE_ROOM_CREATE_RSP, 1, &u8IEInfo);

                    s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[pstManager->u32RecvIndex], pstManager->au8SendBuf,
                        pstManager->u32SendLen, 0, (GS_Sockaddr_S *)&pstManager->stRecvAddr, sizeof(GS_SockaddrIn_S));
                    if (s32Ret != pstManager->u32SendLen)
                    {
                        GS_ERROR("GS_SOCK_Sendto error");
                    }
                }
            }

            break;
        }

        case GS_APP_IE_ROOM_ENTER:
        {
            u32Tmp = GS_APP_GET_U16(pu8Data);

            if (pstInfo->u16RoomIndex > 0)
            {
                /* 已经在房间了 */
                pstRoom = &pstManager->astRoomInfo[pstInfo->u16RoomIndex];

                GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);

                u8IEInfo = (pstRoom->u16RoomID == u32Tmp) ? GS_APP_IE_INFO_IN_SAME_ROOM : GS_APP_IE_INFO_IN_OTHER_ROOM;
                GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                    GS_APP_IE_ROOM_ENTER_RSP, 1, &u8IEInfo);
                GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                    GS_APP_IE_ROOM_INFO, sizeof(GS_APP_RoomInfo_S), &pstRoom);

                s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[pstManager->u32RecvIndex], pstManager->au8SendBuf,
                    pstManager->u32SendLen, 0, (GS_Sockaddr_S *)&pstManager->stRecvAddr, sizeof(GS_SockaddrIn_S));
                if (s32Ret != pstManager->u32SendLen)
                {
                    GS_ERROR("GS_SOCK_Sendto error");
                }

                break;
            }

            for (i=1; i<GS_APP_MAX_ROOM_NUM; i++)
            {
                pstRoom = &pstManager->astRoomInfo[i];
                if (pstRoom->u16RoomID == u32Tmp)
                    break;
            }

            if (i < GS_APP_MAX_ROOM_NUM)
            {
                /* 找到房间 */

                for (i=0; i<GS_APP_ROOM_PLAYER_MAX; i++)
                {
                    pstPlayer = &pstRoom->astPlayerInfo[i];
                    if (pstPlayer->u8IsUsed == 0)
                    {
                        pstPlayer->u8IsUsed = 1;
                        GS_Memcpy(pstPlayer->szName, pstInfo->szPlayerName, GS_APP_PLAYER_NAME_SIZE + 8);
                        pstPlayer->u16ClientIndex = pstManager->u32RecvIndex;

                        pstInfo->u16RoomIndex = pstRoom->u16RoomIndex;
                        break;
                    }
                }

                /* 判断房间是否满员 */
                if (i == GS_APP_ROOM_PLAYER_MAX)
                {
                    GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);

                    u8IEInfo = GS_APP_IE_INFO_PLAYER_FULL;
                    GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                        GS_APP_IE_ROOM_ENTER_RSP, 1, &u8IEInfo);

                    s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[pstManager->u32RecvIndex], pstManager->au8SendBuf,
                        pstManager->u32SendLen, 0, (GS_Sockaddr_S *)&pstManager->stRecvAddr, sizeof(GS_SockaddrIn_S));
                    if (s32Ret != pstManager->u32SendLen)
                    {
                        GS_ERROR("GS_SOCK_Sendto error");
                    }

                    break;
                }

                /* 发送响应 */
                GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);

                u8IEInfo = GS_APP_IE_INFO_ENTER_ROOM;
                GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                    GS_APP_IE_ROOM_ENTER_RSP, 1, &u8IEInfo);
                GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                    GS_APP_IE_ROOM_INFO, sizeof(GS_APP_RoomInfo_S), &pstRoom);

                s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[pstManager->u32RecvIndex], pstManager->au8SendBuf,
                    pstManager->u32SendLen, 0, (GS_Sockaddr_S *)&pstManager->stRecvAddr, sizeof(GS_SockaddrIn_S));
                if (s32Ret != pstManager->u32SendLen)
                {
                    GS_ERROR("GS_SOCK_Sendto error");
                }

                /* 给房间里的其他玩家也更新 */
                GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);
                
                GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                    GS_APP_IE_ROOM_INFO, sizeof(GS_APP_RoomInfo_S), &pstRoom);

                stAddr.s16Family = GS_AF_INET;

                for (i=0; i<GS_APP_ROOM_PLAYER_MAX; i++)
                {
                    pstPlayer = &pstRoom->astPlayerInfo[i];
                    if ((pstPlayer->u16ClientIndex == pstManager->u32RecvIndex)
                        || (pstPlayer->u8IsUsed != 1))
                    {
                        continue;
                    }

                    stAddr.u16Port = pstPlayer->u16ClientPort;
                    GS_IPV4_SET(stAddr.au8IPv4, pstPlayer->au8ClientIPv4);

                    s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[pstManager->u32RecvIndex], pstManager->au8SendBuf,
                        pstManager->u32SendLen, 0, (GS_Sockaddr_S *)&stAddr, sizeof(GS_SockaddrIn_S));
                    if (s32Ret != pstManager->u32SendLen)
                    {
                        GS_ERROR("GS_SOCK_Sendto error");
                    }
                }
            }
            else
            {
                /* 没找到房间 */
                GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);

                u8IEInfo = GS_APP_IE_INFO_ROOM_NOT_EXIST;
                GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                    GS_APP_IE_ROOM_ENTER_RSP, 1, &u8IEInfo);

                s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[pstManager->u32RecvIndex], pstManager->au8SendBuf,
                    pstManager->u32SendLen, 0, (GS_Sockaddr_S *)&pstManager->stRecvAddr, sizeof(GS_SockaddrIn_S));
                if (s32Ret != pstManager->u32SendLen)
                {
                    GS_ERROR("GS_SOCK_Sendto error");
                }
            }

            break;
        }

        case GS_APP_IE_ROOM_EXIT:
        {
            /* 找到房间 */
            GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);

            u8IEInfo = (pstInfo->u16RoomIndex > 0) ? GS_APP_IE_INFO_EXIT_ROOM : GS_APP_IE_INFO_NOT_IN_ROOM;
            GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                GS_APP_IE_ROOM_EXIT_RSP, 1, &u8IEInfo);

            s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[pstManager->u32RecvIndex], pstManager->au8SendBuf,
                pstManager->u32SendLen, 0, (GS_Sockaddr_S *)&pstManager->stRecvAddr, sizeof(GS_SockaddrIn_S));
            if (s32Ret != pstManager->u32SendLen)
            {
                GS_ERROR("GS_SOCK_Sendto error");
            }

            if (pstInfo->u16RoomIndex > 0)
            {
                /* 所有玩家退出则释放房间 */

                pstRoom = &pstManager->astRoomInfo[pstInfo->u16RoomIndex];
                for (i=0; i<GS_APP_ROOM_PLAYER_MAX; i++)
                {
                    pstPlayer = &pstRoom->astPlayerInfo[i];
                    if ((pstPlayer->u8IsUsed == 1)
                        && (pstPlayer->u16ClientIndex != pstManager->u32RecvIndex))
                    {
                        break;
                    }
                }

                if (i == GS_APP_ROOM_PLAYER_MAX)
                {
                    GS_Memset(pstRoom, 0, sizeof(GS_APP_RoomInfo_S));
                }

                pstInfo->u16RoomIndex = 0;
            }

            break;
        }

        case GS_APP_IE_HOLE_REQ:
        case GS_APP_IE_HOLE_RSP:
        case GS_APP_IE_ROOM_INFO:
        {
            GS_INFO("ignore type(%u) len(%u)", u8Type, u32Len);
            break;
        }

        default:
        {
            GS_INFO("unknown type(%u) len(%u)", u8Type, u32Len);
            break;
        }
    }

    return ;
}

static GS_Void GS_APP_StatusListenRootFrameParse(GS_Void *pvData, GS_U8 u8Type, GS_U32 u32Len, GS_U8 *pu8Data)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_ServerManager_S *pstManager = (GS_ServerManager_S *)pvData;
    GS_ClientInfo_S *pstInfo = GS_NULL;
    GS_U16 u16Port = 0;

    switch (u8Type)
    {
        case GS_APP_IE_CONNECT_REQ:
        {
            s32Ret = APP_GetClientInfoIndex(pstManager);
            if (s32Ret >= GS_APP_MAX_CLIENT_NUM)
            {
                GS_ERROR("client full");
                u16Port = GS_CPU_TO_LE16(0);
            }
            else
            {
                pstInfo = &pstManager->astClientInfo[s32Ret];
                u16Port = GS_CPU_TO_LE16(pstInfo->u16ServerPort);
                pstInfo->u32KeepaliveTimeMs = GS_OS_GetTimeMs();
            }

            GS_APP_FrameInit(pstManager->au8SendBuf, &pstManager->u32SendLen);
            GS_APP_FrameAddIE(pstManager->au8SendBuf, &pstManager->u32SendLen,
                GS_APP_IE_CONNECT_RSP, 2, (GS_U8 *)&u16Port);

            GS_INFO("send connect rsp(%u) to "GS_IPV4_FMT":%u", GS_LE_TO_CPU16(u16Port),
                GS_IPV4_ARG(pstManager->stRecvAddr.au8IPv4), GS_Ntohs(pstManager->stRecvAddr.u16Port));

            s32Ret = GS_SOCK_Sendto(pstManager->as32Socket[0], pstManager->au8SendBuf,
                pstManager->u32SendLen, 0, (GS_Sockaddr_S *)&pstManager->stRecvAddr, sizeof(GS_SockaddrIn_S));
            if (s32Ret != pstManager->u32SendLen)
            {
                GS_ERROR("GS_SOCK_Sendto error");
            }

            break;
        }

        default:
        {
            GS_INFO("unknown type(%u) len(%u)", u8Type, u32Len);
            break;
        }
    }

    return ;
}

static GS_S32 GS_APP_StatusListen(GS_ServerManager_S *pstManager)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_ClientInfo_S *pstInfo = GS_NULL;
    GS_NetEndPoint_S stEP;
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

        s32Ret = GS_SOCK_Select(pstManager->as32Socket, GS_APP_MAX_CLIENT_NUM, 50);
        if (s32Ret == -1)
        {
            GS_ERROR("GS_SOCK_Select error");
            s32Ret = GS_FAILED;
            break;
        }
        else if (s32Ret == -2)
        {
            break;
        }

        pstManager->u32RecvIndex = s32Ret;
        s32Sock = pstManager->as32Socket[pstManager->u32RecvIndex];
        u32AddrLen = sizeof(GS_SockaddrIn_S);
        s32Ret = GS_SOCK_Recvfrom(s32Sock, pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE,
            GS_APP_RECV_BUF_SIZE - GS_APP_BUF_HEAD_RESERVE_SIZE, 0, (GS_Sockaddr_S *)&pstManager->stRecvAddr, &u32AddrLen);
        if (s32Ret < 0)
        {
            GS_ERROR("GS_SOCK_Recv error");
            s32Ret = GS_FAILED;
            break;
        }

        pstManager->u32RecvLen = s32Ret;
        if (0 == pstManager->u32RecvIndex)
        {
            /* 监听端口只处理GS帧 */
            s32Ret = GS_APP_FrameCheck(pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE, pstManager->u32RecvLen);
            if (GS_SUCCESS != s32Ret)
            {
                GS_ERROR("GS_APP_FrameCheck error, len = %u", pstManager->u32RecvLen);
                s32Ret = GS_SUCCESS;
                break;
            }

            GS_APP_FrameParse(pstManager, pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE,
                pstManager->u32RecvLen, GS_APP_StatusListenRootFrameParse);
        }
        else
        {
            /* 传输端口处理GS帧和转发数据帧 */

            /* 判断是否来自客户端 */
            pstInfo = &pstManager->astClientInfo[pstManager->u32RecvIndex];
            if (GS_IPV4_CMP(pstInfo->au8ClientIPv4, pstManager->stRecvAddr.au8IPv4)
                && (pstInfo->u16ClientPort == pstManager->stRecvAddr.u16Port))
            {
                /* 来自客户端只接收GS帧 */
                s32Ret = GS_APP_FrameCheck(pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE, pstManager->u32RecvLen);
                if (GS_SUCCESS != s32Ret)
                {
                    GS_ERROR("GS_APP_FrameCheck error, len = %u", pstManager->u32RecvLen);
                    s32Ret = GS_SUCCESS;
                    break;
                }

                GS_APP_FrameParse(pstManager, pstManager->au8RecvBuf + GS_APP_BUF_HEAD_RESERVE_SIZE,
                    pstManager->u32RecvLen, GS_APP_StatusListenFrameParse);
            }
            else
            {
                /* 来自非客户端数据直接转发到客户端 */

                /* 构建GS转发帧 */
                GS_Memcpy(stEP.au8IPv4, pstManager->stRecvAddr.au8IPv4, GS_IPV4_LEN);
                stEP.u16Port = pstManager->stRecvAddr.u16Port;
                s32Ret = GS_APP_FrameToTransmit(pstManager->au8RecvBuf, &pstManager->u32RecvLen, &stEP);
                if (s32Ret < 0)
                {
                    GS_ERROR("GS_APP_FrameToTransmit error");
                    s32Ret = GS_FAILED;
                    break;
                }

                /* 转发到客户端 */
                GS_Memcpy(pstManager->stRecvAddr.au8IPv4, pstInfo->au8ClientIPv4, GS_IPV4_LEN);
                pstManager->stRecvAddr.u16Port = pstInfo->u16ClientPort;
                s32Ret = GS_SOCK_Sendto(s32Sock, pstManager->au8RecvBuf + s32Ret, pstManager->u32RecvLen, 0,
                        (GS_Sockaddr_S *)&pstManager->stRecvAddr, sizeof(GS_SockaddrIn_S));
                if (s32Ret != pstManager->u32RecvLen)
                {
                    GS_ERROR("GS_SOCK_Send error");
                    s32Ret = GS_FAILED;
                    break;
                }
            }
        }
    }while (0);

    if (GS_FAILED == s32Ret)
    {
        pstManager->u8ServerStatus = EN_GS_SERVER_STATUS_EXIT;
    }

    return s32Ret;
}

static GS_Void *fnThreadMain(GS_Void *pvArg)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_ServerManager_S *pstManager = (GS_ServerManager_S *)pvArg;

    do
    {
        pstManager->u8ServerStatus = EN_GS_SERVER_STATUS_INIT;

        s32Ret = GS_APP_Init(pstManager);
        if (s32Ret != GS_SUCCESS)
        {
            GS_ERROR("GS_APP_Init error");
            break;
        }

        pstManager->u8ServerStatus = EN_GS_SERVER_STATUS_LISTEN;

        while (1)
        {
            if (EN_GS_SERVER_STATUS_EXIT == pstManager->u8ServerStatus)
            {
                break;
            }
            else if (EN_GS_SERVER_STATUS_LISTEN == pstManager->u8ServerStatus)
            {
                GS_APP_StatusListen(pstManager);
            }
            else
            {
                break;
            }
        }
    }while (0);

    GS_INFO("app exit");
    return GS_NULL;
}

GS_Int main(GS_Void)
{
    fnThreadMain(&s_stManager);
    GS_OS_SleepMs(10000);
    return 0;
}

