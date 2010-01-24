/*
 * memset.h
 *   prototypes for memset.c
 *
 */

#ifndef _EGG_COMPAT_MEMSET_H
#define _EGG_COMPAT_MEMSET_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>

#ifndef HAVE_MEMSET
/* Use our own implementation. */
void *memset(void *dest, int c, size_t n);
#endif

/* Use memset instead of bzero.
 */
//#undef bzero
//#define bzero(dest, n)	memset(dest, 0, n)

#endif	/* !__EGG_COMPAT_MEMSET_H */
