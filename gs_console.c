#include "gs_console.h"
#include "gs_stdlib.h"

GS_S32 GS_CONSOLE_ParseCmd(GS_Char *szCmd, GS_S32 *pargc, GS_Char *argv[])
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_S32 s32Count = 0;
    GS_Char *pcTmp = GS_NULL;
    GS_Char *pcPos = GS_NULL;

    do
    {
        if (!szCmd || !pargc || !argv || (*szCmd == '\0'))
        {
            GS_ERROR("bad para");
            s32Ret = GS_FAILED;
            break;
        }

        pcPos = szCmd;
        while (s32Count < *pargc)
        {
            pcTmp = GS_Strchr(pcPos, ' ');
            if (!pcTmp)
            {
                argv[s32Count] = pcPos;
                s32Count++;
                break;
            }

            if ((pcTmp != szCmd) && (*(pcTmp - 1) == '\\'))
            {
                pcPos++;
                continue;
            }

            argv[s32Count] = pcPos;
            *pcTmp = '\0';
            pcPos = pcTmp + 1;
            s32Count++;
        }

        *pargc = s32Count;
    }while (0);

    return s32Ret;
}

GS_S32 GS_CONSOLE_GetInput(GS_Char *szCmd, GS_U32 *pu32Len)
{
    GS_S32 s32Ret = GS_SUCCESS;
    GS_U32 u32Len = 0;

    do
    {
        if (!szCmd || !pu32Len || (*pu32Len < 32))
        {
            GS_ERROR("bad para");
            s32Ret = GS_FAILED;
            break;
        }

        while (1)
        {
            GS_Raw("\n---> ");
            u32Len = *pu32Len - 1;
            GS_Memset(szCmd, 0, u32Len+1);
            GS_Fgets(szCmd, u32Len, stdin);
            u32Len = GS_Strlen(szCmd);

            if (u32Len < 3)
                continue;

            if ('\r' == szCmd[u32Len-1] || '\n' == szCmd[u32Len-1])
            {
                szCmd[u32Len-1] = '\0';
                u32Len--;
            }

            if ('\r' == szCmd[u32Len-1] || '\n' == szCmd[u32Len-1])
            {
                szCmd[u32Len-1] = '\0';
                u32Len--;
            }

            if (' ' == szCmd[u32Len-1])
                continue;

            *pu32Len = u32Len + 1;
            break;
        }
    }while (0);

    return s32Ret;
}
