/*
 * eggmain.h
 *   include file to include most other include files
 *
 */

#ifndef _EGG_MAIN_H
#define _EGG_MAIN_H

/* These should be in a common.h, like it or not... */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#include "garble.h"


#include <stdarg.h>
#define EGG_VARARGS(type, name) (type name, ...)
#define EGG_VARARGS_DEF(type, name) (type name, ...)
#define EGG_VARARGS_START(type, name, list) (va_start(list, name), name)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
#include <sys/types.h>
#include "lang.h"
#include "eggdrop.h"
#include "flags.h"
#ifdef HAVE_ZLIB_H
#  include <zlib.h>
#endif /* HAVE_ZLIB_H */

#include "tclegg.h"
#include "tclhash.h"
#include "chan.h"
#include "compat/compat.h"


/* This macro copies (_len - 1) bytes from _source to _target. The
 * target string is NULL-terminated.
 */
#define strncpyz(_target, _source, _len)	do {			\
	strncpy((_target), (_source), (_len) - 1);			\
	(_target)[(_len) - 1] = 0;					\
} while (0)


#endif				/* _EGG_MAIN_H */
