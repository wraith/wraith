/*
 * main.h
 *   include file to include most other include files
 *
 */

#ifndef _EGG_MAIN_H
#define _EGG_MAIN_H


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "pack/conf.h"

#include "lush.h" /* We seem to need this everywhere... */

#if (((TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION >= 5)) || (TCL_MAJOR_VERSION > 7))
#  define USE_TCL_EVENTS
#  define USE_TCL_FINDEXEC
#  define USE_TCL_PACKAGE
#  define USE_TCL_VARARGS
#endif

#if (TCL_MAJOR_VERSION >= 8)
#  define USE_TCL_OBJ
#endif

#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 1)) || (TCL_MAJOR_VERSION > 8))
#  define USE_TCL_BYTE_ARRAYS
#  define USE_TCL_ENCODING
#endif

#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
#  ifdef CONST
#    define EGG_CONST CONST
#  else
#    define EGG_CONST
#  endif
#else
#  define EGG_CONST
#endif


/* UGH! Why couldn't Tcl pick a standard? */
#if defined(USE_TCL_VARARGS) && (defined(__STDC__) || defined(HAS_STDARG))
#  ifdef HAVE_STDARG_H
#    include <stdarg.h>
#  else
#    ifdef HAVE_STD_ARGS_H
#      include <std_args.h>
#    endif
#  endif
#  define EGG_VARARGS(type, name) (type name, ...)
#  define EGG_VARARGS_DEF(type, name) (type name, ...)
#  define EGG_VARARGS_START(type, name, list) (va_start(list, name), name)
#else
#  include <varargs.h>
#  define EGG_VARARGS(type, name) ()
#  define EGG_VARARGS_DEF(type, name) (va_alist) va_dcl
#  define EGG_VARARGS_START(type, name, list) (va_start(list), va_arg(list,type))
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
#include <sys/types.h>
#include "lush.h"
#include "lang.h"
#include "eggdrop.h"
#include "flags.h"

#ifndef MAKING_MODS
#  include "proto.h"
#endif
#include "cmdt.h"
#include "tclegg.h"
#include "tclhash.h"
#include "chan.h"
#include "users.h"
#include "compat/compat.h"

/* For pre Tcl7.5p1 versions */
#ifndef HAVE_TCL_FREE
#  define Tcl_Free(x) n_free(x, "", 0)
#endif

/* For pre7.6 Tcl versions */
#ifndef TCL_PATCH_LEVEL
#  define TCL_PATCH_LEVEL Tcl_GetVar(interp, "tcl_patchLevel", TCL_GLOBAL_ONLY)
#endif

#ifndef MAKING_MODS
extern struct dcc_table DCC_CHAT, DCC_BOT, DCC_LOST, DCC_SCRIPT, DCC_BOT_NEW,
 DCC_RELAY, DCC_RELAYING, DCC_FORK_RELAY, DCC_PRE_RELAY, DCC_CHAT_PASS,
 DCC_FORK_BOT, DCC_SOCKET, DCC_TELNET_ID, DCC_TELNET_NEW, DCC_TELNET_PW,
 DCC_TELNET, DCC_IDENT, DCC_IDENTWAIT, DCC_DNSWAIT;

#endif

#define iptolong(a)		(0xffffffff & 				\
				 (long) (htonl((unsigned long) a)))
#define fixcolon(x)		do {					\
	if ((x)[0] == ':')			 			\
		(x)++;							\
	else								\
		(x) = newsplit(&(x));					\
} while (0)

/* This macro copies (_len - 1) bytes from _source to _target. The
 * target string is NULL-terminated.
 */
#define strncpyz(_target, _source, _len)	do {			\
	strncpy((_target), (_source), (_len) - 1);			\
	(_target)[(_len) - 1] = 0;					\
} while (0)

#ifdef BORGCUBES

/* For net.c */
#  define O_NONBLOCK	00000004    /* POSIX non-blocking I/O		   */
#endif				/* BORGUBES */

#define strncpy0(a,b,c) { strncpy(a, b, c-1); (a)[c-1]=0; }

#ifdef strncpy
#undef strncpy
#endif


#include "md5/md5.h"

#endif				/* _EGG_MAIN_H */
