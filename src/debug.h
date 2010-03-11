#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/*
 * Undefine this to completely disable context debugging.
 * WARNING: DO NOT send in bug reports if you undefine this!
 */

#undef DEBUG_CONTEXT

#define GET_BUFS 30
#define get_buf_inc() if (++current_get_buf == GET_BUFS) current_get_buf = 0;

/*
 *    Handy aliases for memory tracking and core dumps
 */
#ifdef DEBUG_CONTEXT
#  define Context               eggContext(__FILE__, __LINE__)
#  define ContextNote(note)     eggContextNote(__FILE__, __LINE__, note)
#else
#  define Context               {}
#  define ContextNote(note)     {}
#endif

#define debug0(x)               putlog(LOG_DEBUG,"*",x)
#define debug1(x,a1)            putlog(LOG_DEBUG,"*",x,a1)
#define debug2(x,a1,a2)         putlog(LOG_DEBUG,"*",x,a1,a2)
#define debug3(x,a1,a2,a3)      putlog(LOG_DEBUG,"*",x,a1,a2,a3)
#define debug4(x,a1,a2,a3,a4)   putlog(LOG_DEBUG,"*",x,a1,a2,a3,a4)

#include "net.h"
extern bool		sdebug, segfaulted;
extern size_t		current_get_buf;
extern char		get_buf[GET_BUFS][SGRAB + 5];

void setlimits();
void sdprintf (const char *, ...) __attribute__((format(printf, 1, 2)));
void init_signals();
void init_debug();
void eggContext(const char *, int);
void eggContextNote(const char *, int, const char *);
#endif /* !_DEBUG_H */
