dnl  EGG_FUNC_TIMESPEC()
dnl
AC_DEFUN([EGG_FUNC_TIMESPEC], [
AC_CHECK_TYPE([struct timespec], [AC_DEFINE(HAVE_TIMESPEC)], , [#include <sys/types.h>
#include <sys/time.h>
#include <time.h>])

if test X"$ac_cv_type_struct_timespec" != X"no"; then
 AC_CHECK_MEMBER([struct stat.st_mtim], AC_DEFINE(HAVE_ST_MTIM), [AC_CHECK_MEMBER([struct stat.st_mtimespec], AC_DEFINE([HAVE_ST_MTIMESPEC]))])
 AC_MSG_CHECKING([for two-parameter timespecsub])
 AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/time.h>], [struct timespec ts1, ts2;
ts1.tv_sec = 1; ts1.tv_nsec = 0; ts2.tv_sec = 0; ts2.tv_nsec = 0;
#ifndef timespecsub
#error missing timespecsub
#endif
timespecsub(&ts1, &ts2);],
        [AC_DEFINE(HAVE_TIMESPECSUB2)
        AC_MSG_RESULT(yes)], [AC_MSG_RESULT(no)])
fi
dnl
])
