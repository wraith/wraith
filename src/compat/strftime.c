/*
 * strftime.c
 *   Portable strftime implementation. Uses GNU's strftime().
 *
 */

#include "src/common.h"
#include "strftime.h"

#ifndef HAVE_STRFTIME
#  undef emacs
#  undef _LIBC
#  define strftime	egg_strftime

#  include "gnu_strftime.c"
#else /* HAVE_STRFTIME */

size_t my_strftime(char *s, size_t max, const char *fmt, const struct tm *tm) {
  return strftime(s, max, fmt, tm);
}
#endif	/* !HAVE_STRFTIME */
