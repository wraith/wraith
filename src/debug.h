#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define GET_BUFS 30
#define get_buf_inc() if (++current_get_buf == GET_BUFS) current_get_buf = 0;

#define Context do { \
  simple_snprintf(get_buf[current_get_buf], sizeof(get_buf[current_get_buf]), "Context: %s:%d", __FILE__, __LINE__); \
  get_buf_inc(); \
} while (0)

#define ContextNote(_from, _buf) do { \
  simple_snprintf(get_buf[current_get_buf], sizeof(get_buf[current_get_buf]), "%s: %s", _from, _buf); \
  get_buf_inc(); \
} while(0)


/*
 *    Handy aliases for memory tracking and core dumps
 */
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

#ifdef DEBUG
#undef _assert
void _assert(int, const char *, int, const char *, const char *,
    const char *, ...) __attribute__((format(printf, 6, 7)));
#define _ASSERT(recoverable, cond, msg...) do { \
  if (__predict_false(!(cond))) { \
    _assert(recoverable, __FILE__, __LINE__, __func__, # cond, msg); \
  } \
  Context; \
} while (0)
#else
#define _ASSERT(recoverable, cond, msg...) do { \
} while (0)
#endif
#define ASSERT(cond, msg...) _ASSERT(0, cond, msg)
#define ASSERT0(cond) ASSERT(cond, NULL)
#define WARN(cond, msg...) _ASSERT(1, cond, msg)

#endif /* !_DEBUG_H */
