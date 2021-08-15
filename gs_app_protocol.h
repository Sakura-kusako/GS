#ifndef _GS_APP_PROTOCOL_
#define _GS_APP_PROTOCOL_

#include "gs_base.h"
#include "gs_net.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GS_APP_FRAME_HEAD_LEN   (2)

#define GS_APP_IE_THANSMIT              (0)     // 0(1) + len(1~3) + data(len-6) + IP:Port(6)
#define GS_APP_IE_HEARTBEAT             (1)     // 1(1) + 0(1)
#define GS_APP_IE_HEARTBEAT_RSP         (2)     // 2(1) + 0(1)
#define GS_APP_IE_CONNECT_REQ           (3)     // 3(1) + 0(1)
#define GS_APP_IE_CONNECT_RSP           (4)     // 4(1) + 2(1) + ServerPort(2)
#define GS_APP_IE_TEXT                  (5)     // 5(1) + len(1) + string(len)
#define GS_APP_IE_DEBUG_HEARTBEAT       (6)     // 6(1) + 0(1)
#define GS_APP_IE_DEBUG_HEARTBEAT_RSP   (7)     // 7(1) + 0(1)
#define GS_APP_IE_DEBUG_CMD             (8)     // 8(1) + len(1) + cmd(len)

#define GS_APP_IE_ROOM_CREATE           (9)     // 9(1) + nameLen(1) + name(nameLen)
#define GS_APP_IE_ROOM_CREATE_RSP       (10)    // 10(1) + 1(1) + info(1)
    #define GS_APP_IE_INFO_ALREADY_IN_ROOM  (0)     // 已经在房间了
    #define GS_APP_IE_INFO_CREATE_NEW_ROOM  (1)     // 创建了新房间
    #define GS_APP_IE_INFO_NOT_EMPTY_ROOM   (2)     // 没有空房间了

#define GS_APP_IE_ROOM_EXIT             (11)    // 11(1) + 0(1)
#define GS_APP_IE_ROOM_EXIT_RSP         (12)    // 12(1) + 1(1) + info(1)
    #define GS_APP_IE_INFO_NOT_IN_ROOM      (0)     // 不在房间内
    #define GS_APP_IE_INFO_EXIT_ROOM        (1)     // 退出房间

#define GS_APP_IE_ROOM_ENTER            (13)    // 13(1) + 2(1) + RoomID_LE(2)
#define GS_APP_IE_ROOM_ENTER_RSP        (14)    // 14(1) + 1(1) + info(1)
    #define GS_APP_IE_INFO_IN_SAME_ROOM     (0)     // 已经在房间了
    #define GS_APP_IE_INFO_IN_OTHER_ROOM    (1)     // 在其他房间
    #define GS_APP_IE_INFO_ROOM_NOT_EXIST   (2)     // 房间不存在
    #define GS_APP_IE_INFO_ENTER_ROOM       (3)     // 进入房间
    #define GS_APP_IE_INFO_PLAYER_FULL      (4)     // 房间满员

#define GS_APP_IE_HOLE_REQ              (15)
#define GS_APP_IE_HOLE_RSP              (16)

#define GS_APP_IE_ROOM_INFO             (17)    // 17(1) + roomLen(1) + data(roomLen)

#define GS_APP_BUF_HEAD_RESERVE_SIZE    (8)
#define GS_APP_SEND_BUF_SIZE            (1500 + GS_APP_BUF_HEAD_RESERVE_SIZE)
#define GS_APP_RECV_BUF_SIZE            (1500 + GS_APP_BUF_HEAD_RESERVE_SIZE)

#define GS_APP_PLAYER_NAME_SIZE         (20)
#define GS_APP_ROOM_PLAYER_MAX          (5)
#define GS_APP_ROOM_TEXT_SIZE           (64)

#define GS_APP_GET_U16(p)       ((p)[0] + ((p)[1] << 8))
#define GS_APP_SET_U16(p,x)     ((p)[0]=(x)&0xff,(p)[1]=(x>>8)&0xff)

typedef GS_Void(*GS_APP_FrameParse_F)(GS_Void *pvData, GS_U8 u8Type, GS_U32 u32Len, GS_U8 *pu8Data);

GS_S32 GS_APP_FrameParse(GS_Void *pvData, GS_U8 *pu8Buf, GS_U32 u32Len, GS_APP_FrameParse_F fnFrameParse);
GS_S32 GS_APP_FrameCheck(GS_U8 *pu8Buf, GS_U32 u32Len);
GS_S32 GS_APP_FrameInit(GS_U8 *pu8Buf, GS_U32 *pu32Len);
GS_S32 GS_APP_FrameAddIE(GS_U8 *pu8Buf, GS_U32 *pu32Len, GS_U8 u8IEType, GS_U8 u8IELen, GS_Void *pvIEData);
GS_S32 GS_APP_FrameToTransmit(GS_U8 *pu8Buf, GS_U32 *pu32Len, GS_NetEndPoint_S *pstEndPoint);
GS_S32 GS_APP_FrameGetEndPoint(GS_U8 *pu8Buf, GS_U32 *pu32Len, GS_NetEndPoint_S *pstEndPoint);

typedef struct
{
    GS_Char szName[GS_APP_PLAYER_NAME_SIZE];
    GS_U8   au8ClientIPv4[GS_IPV4_LEN];
    GS_U16  u16ClientPort;
    GS_U16  u16ServerPort;
    GS_U32  u32DelayMs;
    GS_U16  u16ClientIndex;
    GS_U8   u8IsUsed;       ///< 0:空位 1:有人 2:禁止
    GS_U8   u8Zero;
} GS_APP_PlayerInfo_S;

typedef struct
{
    GS_APP_PlayerInfo_S astPlayerInfo[GS_APP_ROOM_PLAYER_MAX];
    GS_U8   au8Text[GS_APP_ROOM_TEXT_SIZE];
    GS_U16  u16RoomIndex;
    GS_U16  u16RoomID;
    GS_U8   u8MasterID;
    GS_U8   u8IsUsed;       ///< 0:空房间 1:正在使用 2:禁止进入
    GS_U8   au8Zero[2];
} GS_APP_RoomInfo_S;

#ifdef __cplusplus
}
#endif
#endif

