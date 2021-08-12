#include "gs_app_protocol.h"
#include "gs_stdlib.h"

GS_S32 GS_APP_FrameParse(GS_Void *pvData, GS_U8 *pu8Buf, GS_U32 u32Len, GS_APP_FrameParse_F fnFrameParse)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_U32 u32Pos = 0;
    GS_U32 u32Tmp = 0;
    GS_U8  u8Type = 0;
    GS_U8 *pu8Data = GS_NULL;

    do
    {
        if (!pu8Buf || !pu8Buf || !fnFrameParse)
        {
            GS_ERROR("bad para");
            s32Ret = GS_FAILED;
            break;
        }

        for (u32Pos = GS_APP_FRAME_HEAD_LEN; u32Pos < u32Len; u32Pos += u32Tmp)
        {
            u8Type = pu8Buf[u32Pos];
            u32Tmp = pu8Buf[u32Pos + 1];
            if (u32Tmp == 0xff)
            {
                u32Tmp = pu8Buf[u32Pos + 2] + (pu8Buf[u32Pos + 3] << 8);
                u32Pos += 4;
            }
            else
            {
                u32Pos += 2;
            }

            pu8Data = GS_NULL;
            if (u32Tmp > 0)
            {
                pu8Data = &pu8Buf[u32Pos];
            }

            fnFrameParse(pvData, u8Type, u32Tmp, pu8Data);
        }
    }while (0);

    return s32Ret;
}

GS_S32 GS_APP_FrameCheck(GS_U8 *pu8Buf, GS_U32 u32Len)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_U32 u32Tmp = 0;
    GS_U32 u32Tmp2 = 0;

    do
    {
        if (!pu8Buf)
        {
            GS_ERROR("bad para");
            s32Ret = GS_FAILED;
            break;
        }

        if (u32Len < GS_APP_FRAME_HEAD_LEN + 2)
        {
            GS_ERROR("u32Len(%u) < 4", u32Len);
            s32Ret = GS_FAILED;
            break;
        }

        if (('G' != pu8Buf[0]) && ('S' != pu8Buf[1]))
        {
            GS_ERROR("not GS frame");
            s32Ret = GS_FAILED;
            break;
        }

        u32Tmp = GS_APP_FRAME_HEAD_LEN;
        while (u32Tmp < u32Len)
        {
            if (u32Tmp + 2 > u32Len)
            {
                break;
            }

            u32Tmp2 = pu8Buf[u32Tmp + 1];
            if (u32Tmp2 == 0xff)
            {
                u32Tmp2 = pu8Buf[u32Tmp + 2] + (pu8Buf[u32Tmp + 3] << 8) + 4;
            }
            else
            {
                u32Tmp2 += 2;
            }

            u32Tmp += u32Tmp2;
            if (u32Tmp > u32Len)
            {
                break;
            }
        }

        if (u32Tmp != u32Len)
        {
            GS_ERROR("check len fail");
            s32Ret = GS_FAILED;
            break;
        }
    }while (0);

    return s32Ret;
}

GS_S32 GS_APP_FrameInit(GS_U8 *pu8Buf, GS_U32 *pu32Len)
{
    GS_S32 s32Ret = GS_SUCCESS;

    do
    {
        if (!pu8Buf || !pu32Len)
        {
            GS_ERROR("bad para");
            s32Ret = GS_FAILED;
            break;
        }

        pu8Buf[0] = 'G';
        pu8Buf[1] = 'S';
        *pu32Len = GS_APP_FRAME_HEAD_LEN;
    }while (0);

    return s32Ret;
}

GS_S32 GS_APP_FrameAddIE(GS_U8 *pu8Buf, GS_U32 *pu32Len, GS_U8 u8IEType, GS_U8 u8IELen, GS_Void *pvIEData)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_U8 *pu8Tmp = GS_NULL;

    do
    {
        if (!pu8Buf || !pu32Len || ((u8IELen > 0) && (!pvIEData)))
        {
            GS_ERROR("bad para");
            s32Ret = GS_FAILED;
            break;
        }

        pu8Tmp = pu8Buf + *pu32Len;
        *pu8Tmp++ = u8IEType;
        *pu8Tmp++ = u8IELen;
        if (u8IELen > 0)
        {
            GS_Memcpy(pu8Tmp, pvIEData, u8IELen);
        }
        *pu32Len += 2 + u8IELen;
    }while (0);

    return s32Ret;
}

GS_S32 GS_APP_FrameToTransmit(GS_U8 *pu8Buf, GS_U32 *pu32Len, GS_NetEndPoint_S *pstEndPoint)
{
    GS_S32 s32Ret = GS_FAILED;
    GS_U8 *pu8Tmp = GS_NULL;
    GS_U8 u16Len = 0;

    do
    {
        if (!pu8Buf || !pu32Len || !pstEndPoint)
        {
            GS_ERROR("bad para");
            s32Ret = GS_FAILED;
            break;
        }

        u16Len = *pu32Len + sizeof(GS_NetEndPoint_S);
        if (u16Len >= 0xff)
        {
            pu8Tmp = &pu8Buf[GS_APP_BUF_HEAD_RESERVE_SIZE - 6];
            pu8Tmp[0] = 'G';
            pu8Tmp[1] = 'S';
            pu8Tmp[2] = GS_APP_IE_THANSMIT;
            pu8Tmp[3] = 0xff;
            pu8Tmp[4] = u16Len & 0xff;
            pu8Tmp[5] = u16Len >> 8;
            u16Len += 6;
            pu8Tmp += 6 + *pu32Len;
            s32Ret = GS_APP_BUF_HEAD_RESERVE_SIZE - 6;
        }
        else
        {
            pu8Tmp = &pu8Buf[GS_APP_BUF_HEAD_RESERVE_SIZE - 4];
            pu8Tmp[0] = 'G';
            pu8Tmp[1] = 'S';
            pu8Tmp[2] = GS_APP_IE_THANSMIT;
            pu8Tmp[3] = (GS_U8)u16Len;
            u16Len += 4;
            pu8Tmp += 4 + *pu32Len;
            s32Ret = GS_APP_BUF_HEAD_RESERVE_SIZE - 4;
        }

        GS_Memcpy(pu8Tmp, pstEndPoint, sizeof(GS_NetEndPoint_S));
        *pu32Len = u16Len;
    }while (0);

    return s32Ret;
}

GS_S32 GS_APP_FrameGetEndPoint(GS_U8 *pu8Buf, GS_U32 *pu32Len, GS_NetEndPoint_S *pstEndPoint)
{
    GS_S32 s32Ret = GS_SUCCESS;

    do
    {
        if (!pu8Buf || !pstEndPoint || !pu32Len || *pu32Len < sizeof(GS_NetEndPoint_S))
        {
            GS_ERROR("bad para");
            s32Ret = GS_FAILED;
            break;
        }

        *pu32Len -= sizeof(GS_NetEndPoint_S);
        GS_Memcpy(pstEndPoint, pu8Buf + *pu32Len, sizeof(GS_NetEndPoint_S));
    }while (0);

    return s32Ret;
}

