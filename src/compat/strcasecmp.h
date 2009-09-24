/*
 * strcasecmp.h
 *   prototypes for strcasecmp.c
 *
 */

#ifndef _EGG_COMPAT_STRCASECMP_H
#define _EGG_COMPAT_STRCASECMP_H

#include "src/common.h"
#include <ctype.h>


#ifndef HAVE_STRCASECMP
/* Use our own implementation. */
int strcasecmp(const char *, const char *);
#endif

#ifndef HAVE_STRNCASECMP
/* Use our own implementation. */
int strncasecmp(const char *, const char *, size_t);
#endif

#endif	/* !__EGG_COMPAT_STRCASECMP_H */
