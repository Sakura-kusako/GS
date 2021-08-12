#ifndef _GS_OS_H_
#define _GS_OS_H_

#include "gs_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef GS_Void *(*GS_OS_ThreadEntry_F)(GS_Void *pvArg);

GS_Void GS_OS_Window_Switch(GS_U8 u8Enable);
GS_Void GS_OS_System(GS_Char *szCmd);
GS_U32 GS_OS_GetTimeMs(GS_Void);
GS_Void GS_OS_SleepMs(GS_U32 u32Ms);
GS_Handle GS_OS_CreateThread(GS_U32 u32StackSize, GS_OS_ThreadEntry_F fnEntry, GS_Void *pvData);
GS_Void GS_OS_Exit(GS_U32 u32Ret);

#ifdef __cplusplus
}
#endif
#endif
