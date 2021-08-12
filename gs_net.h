#ifndef _GS_NET_H_
#define _GS_NET_H_

#include "gs_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GS_IPV4_LEN     (4)

#define GS_IPV4_FMT     "%u.%u.%u.%u"
#define GS_IPV4_ARG(a)  (a)[0],(a)[1],(a)[2],(a)[3]
#define GS_IPV4_CMP(a,b) ((*(GS_U32 *)(a)) == (*(GS_U32 *)(b)))
#define GS_IPV4_SET(a,b) ((*(GS_U32 *)(a)) = (*(GS_U32 *)(b)))

typedef struct
{
    GS_U8  au8IPv4[GS_IPV4_LEN];
    GS_U16 u16Port;
} GS_NetEndPoint_S;

#ifdef __cplusplus
}
#endif
#endif

