/*
 * eggmain.h
 *   include file to include most other include files
 *
 */

#ifndef _EGG_MAIN_H
#define _EGG_MAIN_H


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

#ifndef MAKING_MODS
extern struct dcc_table DCC_CHAT, DCC_BOT, DCC_LOST, DCC_BOT_NEW,
 DCC_RELAY, DCC_RELAYING, DCC_FORK_RELAY, DCC_PRE_RELAY, DCC_CHAT_PASS,
 DCC_FORK_BOT, DCC_SOCKET, DCC_TELNET_ID, DCC_TELNET_NEW, DCC_TELNET_PW,
 DCC_TELNET, DCC_IDENT, DCC_IDENTWAIT, DCC_DNSWAIT;

#endif


/* This macro copies (_len - 1) bytes from _source to _target. The
 * target string is NULL-terminated.
 */
#define strncpyz(_target, _source, _len)	do {			\
	strncpy((_target), (_source), (_len) - 1);			\
	(_target)[(_len) - 1] = 0;					\
} while (0)


#endif				/* _EGG_MAIN_H */
