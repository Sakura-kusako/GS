#ifndef _GS_CONSOLE_H_
#define _GS_CONSOLE_H_

#include "gs_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GS_CONSOLE_ERRNO_SUCCESS            (0)
#define GS_CONSOLE_ERRNO_BAD_PARAMETER      (1)

GS_S32 GS_CONSOLE_ParseCmd(GS_Char *szCmd, GS_S32 *pargc, GS_Char *argv[]);
GS_S32 GS_CONSOLE_GetInput(GS_Char *szCmd, GS_U32 *pu32Len);

#ifdef __cplusplus
}
#endif
#endif

