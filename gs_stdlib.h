#ifndef _GS_STDLIB_H_
#define _GS_STDLIB_H_

#include "gs_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GS_Memset(d,c,n)    memset(d,c,n)
#define GS_Memcpy(d,s,n)    memcpy(d,s,n)
#define GS_Memcmp(d,s,n)    memcmp(d,s,n)
#define GS_Atoi(str)        atoi(str)
#define GS_Strstr(s1,s2)    strstr(s1,s2)
#define GS_Strchr(s,c)      strchr(s,c)
#define GS_Strlen(s)        strlen(s)
#define GS_Strcmp(s1,s2)    strcmp(s1,s2)
#define GS_Snprintf(s,l,f,a...)  snprintf(s,l,f,##a)
#define GS_Fgets(s,l,fd)    fgets(s,l,fd)

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
int   memcmp(const void *s1, const void *s2, size_t n);
int   atoi(const char *nptr);
char *strstr(const char *haystack, const char *needle);
char *strchr(const char *s, int c);
size_t strlen(const char *s);
int   strcmp(const char *s1, const char *s2);
int   snprintf(char *str, size_t size, const char *format, ...);
char *fgets(char *s, int size, FILE *stream);

#ifdef __cplusplus
}
#endif
#endif

