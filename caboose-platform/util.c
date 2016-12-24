#include <caboose/platform.h>

void *memcpy(void *__restrict dst,
             const void *__restrict src,
             size_t len)
{
    char *dstdata = dst;
    const char *srcdata = src;
    for (size_t i = 0; i < len; i++) {
        dstdata[i] = srcdata[i];
    }

    return dst;
}

void *memset(void *s, int c, size_t len)
{
    char *data = s;
    for (int i = 0; i < len; i++) {
        data[i] = (char)c;
    }

    return s;
}

char *strcpy(char *destination, const char *source)
{
    char *s = destination;
    while (*source) {
        *s = *source;
        s++;
        source++;
    }
    *s = '\0';

    return destination;
}

int strcmp(const char *s1, const char *s2)
{
    for ( ; *s1 == *s2; s1++, s2++) {
        if (*s1 == '\0') {
            return 0;
        }
    }

    return (*s1 < *s2) ? -1 : 1;
}

size_t strlen(const char *s)
{
    size_t len = 0;
    while (*s) {
        len++;
        s++;
    }

    return len;
}
