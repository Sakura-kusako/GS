#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>

#include "gs_os.h"

GS_Void GS_OS_Window_Switch(GS_U8 u8Enable)
{
    GS_UNUSED(u8Enable);
    return ;
}

GS_Void GS_OS_System(GS_Char *szCmd)
{
    system(szCmd);
}

GS_U32 GS_OS_GetTimeMs(GS_Void)
{
    struct timeval tv;
    gettimeofday(&tv, GS_NULL);
    return (GS_U32)((tv.tv_sec)*1000 + (tv.tv_usec)/1000);
}

GS_Void GS_OS_SleepMs(GS_U32 u32Ms)
{
    struct timeval tv;
    tv.tv_sec = u32Ms / 1000;
    tv.tv_usec = u32Ms % 1000 * 1000;
    select(0, GS_NULL, GS_NULL, GS_NULL, &tv);
    return ;
}

GS_Handle GS_OS_CreateThread(GS_U32 u32StackSize, GS_OS_ThreadEntry_F fnEntry, GS_Void *pvData)
{
    pthread_t ntid;
    GS_Int s32Ret = 0;

    GS_UNUSED(u32StackSize);

    s32Ret = pthread_create(&ntid, GS_NULL, fnEntry, pvData);
    if (s32Ret != 0)
    {
        GS_ERROR("pthread_create error");
    }

    return (GS_Handle)ntid;
}

GS_Void GS_OS_Exit(GS_U32 u32Ret)
{
    exit(u32Ret);
}

