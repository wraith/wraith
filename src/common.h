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


#include "garble.h"
#include "conf.h"
#include "debug.h"
#include "eggdrop.h"
#include "flags.h"
#include "log.h"	/* putlog() */
#include "dccutil.h"	/* dprintf() */
#include "chan.h"
#include "compat/compat.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
#include "lang.h"
#include "tclegg.h"


/* This macro copies (_len - 1) bytes from _source to _target. The
 * target string is NULL-terminated.
 */
#define strncpyz(_target, _source, _len)	do {			\
	strncpy((_target), (_source), (_len) - 1);			\
	(_target)[(_len) - 1] = 0;					\
} while (0)


#endif				/* _COMMON_H */
