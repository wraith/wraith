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
#  define strftime	strftime

#  include "gnu_strftime.c"
#endif	/* !HAVE_STRFTIME */
