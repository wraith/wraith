#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if !HAVE_SIGACTION             /* old "weird signals" */
#  define sigaction sigvec
#  ifndef sa_handler
#    define sa_handler sv_handler
#    define sa_mask sv_mask
#    define sa_flags sv_flags
#  endif
#endif

#if !HAVE_SIGEMPTYSET
/* and they probably won't have sigemptyset, dammit */
#  define sigemptyset(x) ((*(int *)(x))=0)
#endif

/*
 * Undefine this to completely disable context debugging.
 * WARNING: DO NOT send in bug reports if you undefine this!
 */

#define DEBUG_CONTEXT

/*
 *    Handy aliases for memory tracking and core dumps
 */
#ifdef DEBUG_CONTEXT
#  define Context               eggContext(__FILE__, __LINE__, NULL)
#  define ContextNote(note)     eggContextNote(__FILE__, __LINE__, NULL, note)
#else
#  define Context               {}
#  define ContextNote(note)     {}
#endif

#ifdef DEBUG_ASSERT
#  define Assert(expr)  do {                                            \
        if (!(expr))                                                    \
                eggAssert(__FILE__, __LINE__, NULL);                    \
} while (0)
#else
#  define Assert(expr)  do {    } while (0)
#endif

#define debug0(x)               putlog(LOG_DEBUG,"*",x)
#define debug1(x,a1)            putlog(LOG_DEBUG,"*",x,a1)
#define debug2(x,a1,a2)         putlog(LOG_DEBUG,"*",x,a1,a2)
#define debug3(x,a1,a2,a3)      putlog(LOG_DEBUG,"*",x,a1,a2,a3)
#define debug4(x,a1,a2,a3,a4)   putlog(LOG_DEBUG,"*",x,a1,a2,a3,a4)


extern int		sdebug;

void setlimits();
void sdprintf (char *, ...);
void init_signals();
void init_debug();
void eggContext(const char *, int, const char *);
void eggContextNote(const char *, int, const char *, const char *);
void eggAssert(const char *, int, const char *);

#endif /* !_DEBUG_H */
