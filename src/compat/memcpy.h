/*
 * memcpy.h
 *   prototypes for memcpy.c
 *
 */

#ifndef _EGG_COMPAT_MEMCPY_H
#define _EGG_COMPAT_MEMCPY_H

#include "src/common.h"
#include <string.h>

#ifndef HAVE_MEMCPY
/* Use our own implementation. */
void *egg_memcpy(void *dest, const void *src, size_t n);
#else
#  define egg_memcpy	memcpy
#endif

#endif	/* !__EGG_COMPAT_MEMCPY_H */
