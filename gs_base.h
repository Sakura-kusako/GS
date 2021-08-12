#ifndef _GS_BASE_H_
#define _GS_BASE_H_

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(GS_BIG_ENDIAN) && !defined(GS_LITTLE_ENDIAN)
#define GS_LITTLE_ENDIAN 1
#endif

#if !defined(GS_PLATFORM_WINDOWS) && !defined(GS_PLATFORM_LINUX)
#define GS_PLATFORM_LINUX 1
#endif

typedef void           *GS_Handle;
typedef void            GS_Void;
typedef int             GS_Int;
typedef char            GS_Char;

typedef unsigned int    GS_U32;
typedef signed int      GS_S32;
typedef unsigned short  GS_U16;
typedef signed short    GS_S16;
typedef unsigned char   GS_U8;
typedef signed char     GS_S8;

#define GS_UNUSED(x)        ((GS_Void)(x))
#define GS_AVOID_WARNING(x) ((GS_Void)(x))

#define GS_SUCCESS      (0)
#define GS_FAILED       (-1)
#define GS_NULL         ((GS_Void *)0)

#define GS_SWAP_ENDIAN_16(x) ((GS_U16)(((x) << 8) | (((x) >> 8) & 0xFF)))
#define GS_SWAP_ENDIAN_32(x) ((GS_U32)(((x) << 24) | (((x) << 8) & 0xFF0000) \
                              | (((x) >> 8) & 0xFF00) | (((x) >> 24) & 0xFF)))

#ifdef GS_LITTLE_ENDIAN
#define GS_CPU_TO_LE16(x)   ((GS_U16)(x))
#define GS_CPU_TO_LE32(x)   ((GS_U32)(x))
#define GS_CPU_TO_BE16(x)   GS_SWAP_ENDIAN_16(x)
#define GS_CPU_TO_BE32(x)   GS_SWAP_ENDIAN_32(x)
#define GS_LE_TO_CPU16(x)   ((GS_U16)(x))
#define GS_LE_TO_CPU32(x)   ((GS_U32)(x))
#define GS_BE_TO_CPU16(x)   GS_SWAP_ENDIAN_16(x)
#define GS_BE_TO_CPU32(x)   GS_SWAP_ENDIAN_32(x)
#else
#define GS_CPU_TO_LE16(x)   GS_SWAP_ENDIAN_16(x)
#define GS_CPU_TO_LE32(x)   GS_SWAP_ENDIAN_32(x)
#define GS_CPU_TO_BE16(x)   ((GS_U16)(x))
#define GS_CPU_TO_BE32(x)   ((GS_U32)(x))
#define GS_LE_TO_CPU16(x)   GS_SWAP_ENDIAN_16(x)
#define GS_LE_TO_CPU32(x)   GS_SWAP_ENDIAN_32(x)
#define GS_BE_TO_CPU16(x)   ((GS_U16)(x))
#define GS_BE_TO_CPU32(x)   ((GS_U32)(x))
#endif

#define GS_Raw(fmt,arg...)      printf(fmt,##arg)
#define GS_Printf(fmt,arg...)   GS_Raw(fmt,##arg)
#define GS_INFO(fmt,arg...)     GS_Printf("[%s][%d] "fmt"\n", __FUNCTION__, __LINE__, ##arg)
#define GS_ERROR(fmt,arg...)    GS_Printf("[%s][%d][ERROR] "fmt"\n", __FUNCTION__, __LINE__, ##arg)


#ifdef __cplusplus
}
#endif

#endif
