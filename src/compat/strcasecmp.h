/*
 * strcasecmp.h
 *   prototypes for strcasecmp.c
 *
 */

#ifndef _EGG_COMPAT_STRCASECMP_H
#define _EGG_COMPAT_STRCASECMP_H

#include "src/eggmain.h"
#include <ctype.h>


#ifndef HAVE_STRCASECMP
/* Use our own implementation. */
int egg_strcasecmp(const char *, const char *);
#else
#  define egg_strcasecmp	strcasecmp
#endif

#ifndef HAVE_STRNCASECMP
/* Use our own implementation. */
int egg_strncasecmp(const char *, const char *, size_t);
#else
#  define egg_strncasecmp	strncasecmp
#endif

#endif	/* !__EGG_COMPAT_STRCASECMP_H */
