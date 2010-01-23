/*
 * strftime.h
 *   header file for strftime.c
 *
 */

#ifndef _EGG_COMPAT_STRFTIME_H_
#define _EGG_COMPAT_STRFTIME_H_

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <time.h>

/* Use the system libraries version of strftime() if available. Otherwise
 * use our own.
 */
#ifndef HAVE_STRFTIME
size_t strftime(char *s, size_t maxsize, const char *format, const struct tm *tp);
#endif

#endif	/* !_EGG_COMPAT_STRFTIME_H_ */
