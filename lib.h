/******************************************************************************
 * @file            lib.h
 *****************************************************************************/
#ifndef     _LIB_H
#define     _LIB_H

#include    "stdint.h"

char *xstrdup (const char *__p);
char *xstrndup (const char *__p, unsigned long __size);

uint64_t array_to_integer (unsigned char *arr, int size, int bigendian);
void parse_args (int argc, char **argv, int optind);

void *xmalloc (unsigned long __size);
void *xrealloc (void *__ptr, unsigned long __size);

#endif      /* _LIB_H */
