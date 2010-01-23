/*
 * memcpy.h
 *   prototypes for memcpy.c
 *
 */

#ifndef _EGG_COMPAT_MEMCPY_H
#define _EGG_COMPAT_MEMCPY_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>

#ifndef HAVE_MEMCPY
/* Use our own implementation. */
void *memcpy(void *dest, const void *src, size_t n);
#endif

#endif	/* !__EGG_COMPAT_MEMCPY_H */
