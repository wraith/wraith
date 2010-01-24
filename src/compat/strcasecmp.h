/*
 * strcasecmp.h
 *   prototypes for strcasecmp.c
 *
 */

#ifndef _EGG_COMPAT_STRCASECMP_H
#define _EGG_COMPAT_STRCASECMP_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

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
