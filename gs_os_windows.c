#include <windef.h>
#include <WinBase.h>
#include <windows.h>

#include "gs_os.h"

GS_Void GS_OS_Window_Switch(GS_U8 u8Enable)
{
    static HWND hwnd = GS_NULL;

    if (!hwnd)
    {
        hwnd = FindWindow("ConsoleWindowClass", NULL);
    }

    if (hwnd)
    {
        u8Enable = (1 == u8Enable) ? SW_SHOWNOACTIVATE : SW_HIDE;
        ShowWindow(hwnd, u8Enable); //设置指定窗口的显示状态
    }
}

GS_Void GS_OS_System(GS_Char *szCmd)
{
    system(szCmd);
}

GS_U32 GS_OS_GetTimeMs(GS_Void)
{
    return GetTickCount();
}

GS_Void GS_OS_SleepMs(GS_U32 u32Ms)
{
    return Sleep(u32Ms);
}

typedef struct
{
    GS_OS_ThreadEntry_F fnEntry;
    GS_Void *pvData;
    GS_U32  u32IsRun;
} OS_ThreadParam_S;

static DWORD WINAPI OS_ThreadEntry(LPVOID p)
{
    OS_ThreadParam_S *pstParam = (OS_ThreadParam_S *)p;

    GS_OS_ThreadEntry_F fnEntry = pstParam->fnEntry;
    GS_Void *pvData = pstParam->pvData;
    pstParam->u32IsRun = 1;

    fnEntry(pvData);
    return 0;
}

GS_Handle GS_OS_CreateThread(GS_U32 u32StackSize, GS_OS_ThreadEntry_F fnEntry, GS_Void *pvData)
{
    OS_ThreadParam_S stParam;
    HANDLE hThread;
    DWORD  threadId;

    GS_UNUSED(u32StackSize);

    stParam.fnEntry = fnEntry;
    stParam.pvData = pvData;
    stParam.u32IsRun = 0;

    hThread = CreateThread(NULL, 0, OS_ThreadEntry, &stParam, 0, &threadId);
    if (!hThread)
    {
        GS_ERROR("CreateThread error");
    }

    while (0 == stParam.u32IsRun)
        GS_OS_SleepMs(50);

    return hThread;
}

GS_Void GS_OS_Exit(GS_U32 u32Ret)
{
    exit(u32Ret);
}

