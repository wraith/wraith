/*
 * common.h
 *   include file to include most other include files
 *
 */

#ifndef _COMMON_H
#define _COMMON_H

/* These should be in a common.h, like it or not... */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/* AUTHHASH is pointless without AUTHCMDS. */
#ifdef S_AUTHHASH
# ifndef S_AUTHCMDS
#  undef S_AUTHHASH
# endif /* !S_AUTHCMDS */
#endif /* S_AUTHHASH */

#include "bits.h"
#include "garble.h"
#include "conf.h"
#include "debug.h"
#include "eggdrop.h"
#include "flags.h"
#include "log.h"
#include "dccutil.h"
#include "chan.h"
#include "compat/compat.h"

#ifdef CYGWIN_HACKS
#  include <windows.h>
#endif /* CYGWIN_HACKS */
#include <sys/param.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
#include "lang.h"


/* This macro copies (_len - 1) bytes from _source to _target. The
 * target string is NULL-terminated.
 */
#define strncpyz(_target, _source, _len)	do {			\
	strncpy((_target), (_source), (_len) - 1);			\
	(_target)[(_len) - 1] = 0;					\
} while (0)


#ifdef WIN32
# undef exit
# define exit(x) ExitProcess(x)

//# undef system
//  int my_system(const char *);		/* in shell.c */
//# define system(_run) 	my_system(_run)
#endif /* WIN32 */

#endif				/* _COMMON_H */
