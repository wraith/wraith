dnl aclocal.m4
dnl   macros autoconf uses when building configure from configure.in
dnl
dnl

dnl  EGG_CHECK_CC()
dnl
dnl  FIXME: make a better test
dnl
AC_DEFUN(EGG_CHECK_CC, [dnl
if test "${cross_compiling-x}" = "x"
then
  cat << 'EOF' >&2
configure: error:

  This system does not appear to have a working C compiler.
  A working C compiler is required to compile Eggdrop.

EOF
  exit 1
fi
])dnl

dnl  EGG_IPV6_OPTIONS()
dnl
AC_DEFUN(EGG_IPV6_OPTIONS, [dnl
AC_MSG_CHECKING(whether or not you disabled IPv6 support)
AC_ARG_ENABLE(ipv6, [  --disable-ipv6           disable IPv6 support],
[ ac_cv_dipv6="yes"
  AC_MSG_RESULT(yes)
],
[ ac_cv_dipv6="no"
  if test "$egg_cv_ipv6_supported" = "no"; then
    ac_cv_dipv6="no"
  fi
  AC_MSG_RESULT($ac_cv_dipv6)
])
if test "$ac_cv_dipv6" = "no"; then
  AC_DEFINE(USE_IPV6, 1, [Define if you want ipv6 support])
fi
])dnl


dnl  EGG_CHECK_SOCKLEN_T()
dnl
AC_DEFUN(EGG_CHECK_SOCKLEN_T, [dnl
AC_MSG_CHECKING(for socklen_t)
AC_CACHE_VAL(egg_cv_socklen_t,[
  AC_TRY_RUN([
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main()
{
  socklen_t test = sizeof(int);

  return 0;
}
  ],
egg_cv_socklen_t=yes, egg_cv_socklen_t=no, egg_cv_socklen_t=no)])
if test "$egg_cv_socklen_t" = "yes"; then
  AC_DEFINE(HAVE_SOCKLEN_T, 1, [Define if you have support for socklen_t])
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi
])dnl


dnl  EGG_CHECK_CCPIPE()
dnl
dnl  Checks whether the compiler supports the `-pipe' flag, which
dnl  speeds up the compilation.
AC_DEFUN(EGG_CHECK_CCPIPE, [dnl
if test -z "$no_pipe"
then
  if test -n "$GCC"
  then
    AC_CACHE_CHECK(whether the compiler understands -pipe, egg_cv_var_ccpipe, [dnl
      ac_old_CC="$CC"
      CC="$CC -pipe"
      AC_TRY_COMPILE(,, egg_cv_var_ccpipe="yes", egg_cv_var_ccpipe="no")
      CC="$ac_old_CC"
    ])
    if test "$egg_cv_var_ccpipe" = "yes"
    then
      CC="$CC -pipe"
    fi
  fi
fi
])dnl

dnl  EGG_CHECK_CCSTATIC()
dnl
dnl  Checks whether the compiler supports the `-static' flag.
AC_DEFUN(EGG_CHECK_CCSTATIC, [dnl
if test -z "$no_static"
then
  if test -n "$GCC"
  then
    AC_CACHE_CHECK(whether the compiler understands -static, egg_cv_var_ccstatic, [dnl
      ac_old_CC="$CC"
      CC="$CC -static"
      AC_TRY_COMPILE(,, egg_cv_var_ccstatic="yes", egg_cv_var_ccstatic="no")
      CC="$ac_old_CC"
      
    ])
    if test "$egg_cv_var_ccstatic" = "yes"
    then
      CCDEBUG="$CC"
      CC="$CC -static"
    else
      cat << 'EOF' >&2
configure: error:

  Your C compiler does not support -static.
  This compile flag is required for the botpack.

EOF
  exit 1
    fi
  fi
fi
])dnl

dnl EGG_PROG_HEAD_1()
dnl
AC_DEFUN(EGG_PROG_HEAD_1,
[cat << 'EOF' > conftest.head
a
b
c
EOF

for ac_prog in 'head -1' 'head -n 1' 'sed 1q';
do
  AC_MSG_CHECKING([whether $ac_prog works])
  AC_CACHE_VAL(ac_cv_prog_HEAD_1,
[  if test -n "$HEAD_1"
  then
    ac_cv_prog_HEAD_1="$HEAD_1" # Let the user override the test.
  else
   if test "`cat conftest.head | $ac_prog`" = "a";
   then
     AC_MSG_RESULT([yes])
     ac_cv_prog_HEAD_1=$ac_prog
   else
     AC_MSG_RESULT([no])
   fi
  fi])dnl
  test -n "$ac_cv_prog_HEAD_1" && break
done

if test "${ac_cv_prog_HEAD_1-x}" = "x"
then
  cat << 'EOF' >&2
configure: error:

  This system seems to lack a working 'head -1' or 'head -n 1' command.
  A working 'head -1' (or equivalent) command is required to compile Eggdrop.

EOF
  exit 1
fi

rm -f conftest.head
HEAD_1=$ac_cv_prog_HEAD_1
AC_SUBST(HEAD_1)dnl
])dnl


dnl  EGG_PROG_AWK()
dnl
AC_DEFUN(EGG_PROG_AWK, [dnl
# awk is needed for Tcl library and header checks, and eggdrop version subst
AC_PROG_AWK
if test "${AWK-x}" = "x"
then
  cat << 'EOF' >&2
configure: error:

  This system seems to lack a working 'awk' command.
  A working 'awk' command is required to compile Eggdrop.

EOF
  exit 1
fi
])dnl


dnl  EGG_PROG_BASENAME()
dnl
AC_DEFUN(EGG_PROG_BASENAME, [dnl
# basename is needed for Tcl library and header checks
AC_CHECK_PROG(BASENAME, basename, basename)
if test "${BASENAME-x}" = "x"
then
  cat << 'EOF' >&2
configure: error:

  This system seems to lack a working 'basename' command.
  A working 'basename' command is required to compile Eggdrop.

EOF
  exit 1
fi
])dnl


dnl  EGG_CHECK_OS()
dnl
dnl  FIXME/NOTICE:
dnl    This function is obsolete. Any NEW code/checks should be written
dnl    as individual tests that will be checked on ALL operating systems.
dnl
AC_DEFUN(EGG_CHECK_OS, [dnl
LINUX=no
IRIX=no
SUNOS=no
HPUX=no
EGG_CYGWIN=no

AC_CACHE_CHECK(system type, egg_cv_var_system_type, egg_cv_var_system_type=`$UNAME -s`)
AC_CACHE_CHECK(system release, egg_cv_var_system_release, egg_cv_var_system_release=`$UNAME -r`)

case "$egg_cv_var_system_type" in
  BSD/OS)
    case "`echo $egg_cv_var_system_release | cut -d . -f 1`" in
      2)
      ;;
      *)
        CFLAGS="$CFLAGS -Wall"
      ;;
    esac
  ;;
  CYGWI*)
    case "`echo $egg_cv_var_system_release | cut -c 1-3`" in
      1.*)
        AC_PROG_CC_WIN32
        CC="$CC $WIN32FLAGS"
        AC_MSG_CHECKING(for /usr/lib/binmode.o)
        if test -r /usr/lib/binmode.o
        then
          AC_MSG_RESULT(yes)
          LIBS="$LIBS /usr/lib/binmode.o"
        else
          AC_MSG_RESULT(no)
          AC_MSG_WARN(Make sure the directory Eggdrop is installed into is mounted in binary mode.)
        fi
      ;;
      *)
        AC_MSG_WARN(Make sure the directory Eggdrop is installed into is mounted in binary mode.)
      ;;
    esac
    EGG_CYGWIN=yes
    AC_DEFINE(CYGWIN_HACKS, 1, [Define if running under cygwin])
  ;;
  HP-UX)
    HPUX=yes
    AC_DEFINE(HPUX_HACKS, 1, [Define if running on hpux that supports dynamic linking])dnl
    if test "`echo $egg_cv_var_system_release | cut -d . -f 2`" = "10"
    then
      AC_DEFINE(HPUX10_HACKS, 1, [Define if running on hpux 10.x])dnl
    fi
  ;;
  dell)
    AC_MSG_RESULT(Dell SVR4)
  ;;
  IRIX)
    IRIX=yes
  ;;
  Ultrix)
    SHELL=/bin/sh5
  ;;
  SINIX*)
  ;;
  BeOS)
  ;;
  Linux)
    LINUX=yes
    CFLAGS="$CFLAGS -Wall"
  ;;
  Lynx)
  ;;
  QNX)
  ;;
  OSF1)
    case "`echo $egg_cv_var_system_release | cut -d . -f 1`" in
      V*)
        # FIXME: we should check this in a separate test
        # Digital OSF uses an ancient version of gawk
        if test "$AWK" = "gawk"
        then
          AWK=awk
        fi
      ;;
      1.0|1.1|1.2)
        AC_DEFINE(OSF1_HACKS, 1, [Define if running on OSF/1 platform])dnl
      ;;
      1.*)
        AC_DEFINE(OSF1_HACKS, 1, [Define if running on OSF/1 platform])dnl
      ;;
      *)
      ;;
    esac
    AC_DEFINE(STOP_UAC, 1, [Define if running on OSF/1 platform])dnl
    AC_DEFINE(BROKEN_SNPRINTF, 1, [Define to use Eggdrop's snprintf functions without regard to HAVE_SNPRINTF])dnl
  ;;
  SunOS)
    if ! test "`echo $egg_cv_var_system_release | cut -d . -f 1`" = "5"
      then
      # SunOS 4
      SUNOS=yes
    fi
  ;;
  *BSD)
    # FreeBSD/OpenBSD/NetBSD
  ;;
  *)
    AC_MSG_CHECKING(if system is Mach based)
    if test -r /mach
    then
      AC_MSG_RESULT(yes)
      AC_DEFINE(BORGCUBES, 1, [Define if running on NeXT Step])dnl
    else
      AC_MSG_RESULT(no)
      AC_MSG_CHECKING(if system is QNX)
      if test -r /cmds
      then
        AC_MSG_RESULT(yes)
      else
        AC_MSG_RESULT(no)
        AC_MSG_RESULT(Something unknown!)
        AC_MSG_RESULT([If you get dynamic modules to work, be sure to let the devel team know HOW :)])
      fi
    fi
  ;;
esac
])dnl


dnl  EGG_CHECK_LIBS()
dnl
AC_DEFUN(EGG_CHECK_LIBS, [dnl
# FIXME: this needs to be fixed so that it works on IRIX
if test "$IRIX" = "yes"
then
  AC_MSG_WARN(Skipping library tests because they CONFUSE Irix.)
else
  AC_CHECK_LIB(socket, socket)
  AC_CHECK_LIB(nsl, connect)
  AC_CHECK_LIB(dns, gethostbyname)
  AC_CHECK_LIB(z, gzopen, ZLIB="-lz")
  AC_CHECK_LIB(ssl, SSL_accept, SSL="-lssl -lcrypto", SSL="", -lcrypto) 
  AC_CHECK_LIB(dl, dlopen)
  AC_CHECK_LIB(m, tan, EGG_MATH_LIB="-lm")
  # This is needed for Tcl libraries compiled with thread support
  AC_CHECK_LIB(pthread, pthread_mutex_init, [dnl
  ac_cv_lib_pthread_pthread_mutex_init=yes
  ac_cv_lib_pthread="-lpthread"], [dnl
    AC_CHECK_LIB(pthread, __pthread_mutex_init, [dnl
    ac_cv_lib_pthread_pthread_mutex_init=yes
    ac_cv_lib_pthread="-lpthread"], [dnl
      AC_CHECK_LIB(pthreads, pthread_mutex_init, [dnl
      ac_cv_lib_pthread_pthread_mutex_init=yes
      ac_cv_lib_pthread="-lpthreads"], [dnl
        AC_CHECK_FUNC(pthread_mutex_init, [dnl
        ac_cv_lib_pthread_pthread_mutex_init=yes
        ac_cv_lib_pthread=""],
        ac_cv_lib_pthread_pthread_mutex_init=no)])])])
  if test "$SUNOS" = "yes"
  then
    # For suns without yp or something like that
    AC_CHECK_LIB(dl, main)
  else
    if test "$HPUX" = "yes"
    then
      AC_CHECK_LIB(dld, shl_load)
    fi
  fi
fi
])dnl

dnl  EGG_CHECK_FUNC_VSPRINTF()
dnl
AC_DEFUN(EGG_CHECK_FUNC_VSPRINTF, [dnl
AC_CHECK_FUNCS(vsprintf)
if test "$ac_cv_func_vsprintf" = "no"
then
  cat << 'EOF' >&2
configure: error:

  Your system does not have the sprintf/vsprintf libraries.
  These are required to compile almost anything.  Sorry.

EOF
  exit 1
fi
])dnl

dnl  EGG_CHECK_FUNC_UNAME()
dnl
AC_DEFUN(EGG_CHECK_FUNC_UNAME, [dnl
AC_CHECK_FUNCS(uname)
if test "$ac_cv_func_uname" = "no"
then
  cat << 'EOF' >&2
configure: error:

  Your system does not have the uname() function.
  This is required for the botpack.

EOF
  exit 1
fi
])dnl

dnl  EGG_CHECK_ZLIB()
dnl
AC_DEFUN(EGG_CHECK_ZLIB, [dnl
if test "x${ZLIB}" = x; then
  cat >&2 <<EOF
configure: error:

  Your system does not provide a working zlib compression library. 
  It is required.

EOF
  exit 1
else
  if test "${ac_cv_header_zlib_h}" != yes; then
    cat >&2 <<EOF
configure: error:

  Your system does not provide the necessary zlib header file. 
  It is required.

EOF
    exit 1
  fi
fi
])dnl


dnl  EGG_CHECK_SSL()
dnl
AC_DEFUN(EGG_CHECK_SSL, [dnl
if test "x${SSL}" = x; then
  cat >&2 <<EOF
configure: error:

  Your system does not provide a working ssl library. 
  It is required. Download openssl at www.openssl.org

EOF
  exit 1
else
  if test "${ac_cv_header_openssl_ssl_h}" != yes; then
    cat >&2 <<EOF
configure: error:

  Your system does not provide the necessary ssl header file. 
  It is required. Download openssl at www.openssl.org

EOF
    exit 1
  fi
fi
])dnl

dnl  EGG_HEADER_STDC()
dnl
AC_DEFUN(EGG_HEADER_STDC, [dnl
if test "$ac_cv_header_stdc" = "no"
then
  cat << 'EOF' >&2
configure: error:

  Your system must support ANSI C Header files.
  These are required for the language support.  Sorry.

EOF
  exit 1
fi
])dnl


dnl  EGG_TCL_ARG_WITH()
dnl
AC_DEFUN(EGG_TCL_ARG_WITH, [dnl
# oohh new configure --variables for those with multiple Tcl libs
AC_ARG_WITH(tcllib, [  --with-tcllib=PATH      full path to Tcl library], tcllibname="$withval")
AC_ARG_WITH(tclinc, [  --with-tclinc=PATH      full path to Tcl header], tclincname="$withval")

WARN=0
# Make sure either both or neither $tcllibname and $tclincname are set
if test ! "${tcllibname-x}" = "x"
then
  if test "${tclincname-x}" = "x"
  then
    WARN=1
    tcllibname=""
    TCLLIB=""
    TCLINC=""
  fi
else
  if test ! "${tclincname-x}" = "x"
  then
    WARN=1
    tclincname=""
    TCLLIB=""
    TCLINC=""
  fi
fi
if test "$WARN" = 1
then
  cat << 'EOF' >&2
configure: warning:

  You must specify both --with-tcllib and --with-tclinc for them to work.
  configure will now attempt to autodetect both the Tcl library and header...

EOF
fi
])dnl


dnl  EGG_TCL_ENV()
dnl
AC_DEFUN(EGG_TCL_ENV, [dnl
WARN=0
# Make sure either both or neither $TCLLIB and $TCLINC are set
if test ! "${TCLLIB-x}" = "x"
then
  if test "${TCLINC-x}" = "x"
  then
    WARN=1
    WVAR1=TCLLIB
    WVAR2=TCLINC
    TCLLIB=""
  fi
else
  if test ! "${TCLINC-x}" = "x"
  then
    WARN=1
    WVAR1=TCLINC
    WVAR2=TCLLIB
    TCLINC=""
  fi
fi
if test "$WARN" = 1
then
  cat << EOF >&2
configure: warning:

  Environment variable $WVAR1 was set, but I did not detect ${WVAR2}.
  Please set both TCLLIB and TCLINC correctly if you wish to use them.
  configure will now attempt to autodetect both the Tcl library and header...

EOF
fi
])dnl


dnl  EGG_TCL_WITH_TCLLIB()
dnl
AC_DEFUN(EGG_TCL_WITH_TCLLIB, [dnl
# Look for Tcl library: if $tcllibname is set, check there first
if test ! "${tcllibname-x}" = "x"
then
  if test -f "$tcllibname" && test -r "$tcllibname"
  then
    TCLLIB=`echo $tcllibname | sed 's%/[[^/]][[^/]]*$%%'`
    TCLLIBFN=`$BASENAME $tcllibname | cut -c4-`
    TCLLIBEXT=".`echo $TCLLIBFN | $AWK '{j=split([$]1, i, "."); print i[[j]]}'`"
    TCLLIBFNS=`$BASENAME $tcllibname $TCLLIBEXT | cut -c4-`
  else
    cat << EOF >&2
configure: warning:

  The file '$tcllibname' given to option --with-tcllib is not valid.
  configure will now attempt to autodetect both the Tcl library and header...

EOF
    tcllibname=""
    tclincname=""
    TCLLIB=""
    TCLLIBFN=""
    TCLINC=""
    TCLINCFN=""
  fi
fi
])dnl


dnl  EGG_TCL_WITH_TCLINC()
dnl
AC_DEFUN(EGG_TCL_WITH_TCLINC, [dnl
# Look for Tcl header: if $tclincname is set, check there first
if test ! "${tclincname-x}" = "x"
then
  if test -f "$tclincname" && test -r "$tclincname"
  then
    TCLINC=`echo $tclincname | sed 's%/[[^/]][[^/]]*$%%'`
    TCLINCFN=`$BASENAME $tclincname`
  else
    cat << EOF >&2
configure: warning:

  The file '$tclincname' given to option --with-tclinc is not valid.
  configure will now attempt to autodetect both the Tcl library and header...

EOF
    tcllibname=""
    tclincname=""
    TCLLIB=""
    TCLLIBFN=""
    TCLINC=""
    TCLINCFN=""
  fi
fi
])dnl


dnl  EGG_TCL_FIND_LIBRARY()
dnl
AC_DEFUN(EGG_TCL_FIND_LIBRARY, [dnl
# Look for Tcl library: if $TCLLIB is set, check there first
if test "${TCLLIBFN-x}" = "x"
then
  if test ! "${TCLLIB-x}" = "x"
  then
    if test -d "$TCLLIB"
    then
      for tcllibfns in $tcllibnames
      do
        for tcllibext in $tcllibextensions
        do
          if test -r "$TCLLIB/lib$tcllibfns$tcllibext"
          then
            TCLLIBFN="$tcllibfns$tcllibext"
            TCLLIBEXT="$tcllibext"
            TCLLIBFNS="$tcllibfns"
            break 2
          fi
        done
      done
    fi
    if test "${TCLLIBFN-x}" = "x"
    then
      cat << 'EOF' >&2
configure: warning:

  Environment variable TCLLIB was set, but incorrect.
  Please set both TCLLIB and TCLINC correctly if you wish to use them.
  configure will now attempt to autodetect both the Tcl library and header...

EOF
      TCLLIB=""
      TCLLIBFN=""
      TCLINC=""
      TCLINCFN=""
    fi
  fi
fi
])dnl


dnl  EGG_TCL_FIND_HEADER()
dnl
AC_DEFUN(EGG_TCL_FIND_HEADER, [dnl
# Look for Tcl header: if $TCLINC is set, check there first
if test "${TCLINCFN-x}" = "x"
then
  if test ! "${TCLINC-x}" = "x"
  then
    if test -d "$TCLINC"
    then
      for tclheaderfn in $tclheadernames
      do
        if test -r "$TCLINC/$tclheaderfn"
        then
          TCLINCFN="$tclheaderfn"
          break
        fi
      done
    fi
    if test "${TCLINCFN-x}" = "x"
    then
      cat << 'EOF' >&2
configure: warning:

  Environment variable TCLINC was set, but incorrect.
  Please set both TCLLIB and TCLINC correctly if you wish to use them.
  configure will now attempt to autodetect both the Tcl library and header...

EOF
      TCLLIB=""
      TCLLIBFN=""
      TCLINC=""
      TCLINCFN=""
    fi
  fi
fi
])dnl


dnl  EGG_TCL_CHECK_LIBRARY()
dnl
AC_DEFUN(EGG_TCL_CHECK_LIBRARY, [dnl
AC_MSG_CHECKING(for Tcl library)

# Attempt autodetect for $TCLLIBFN if it's not set
if test ! "${TCLLIBFN-x}" = "x"
then
  AC_MSG_RESULT(using $TCLLIB/lib$TCLLIBFN)
else
  for tcllibfns in $tcllibnames
  do
    for tcllibext in $tcllibextensions
    do
      for tcllibpath in $tcllibpaths
      do
        if test -r "$tcllibpath/lib$tcllibfns$tcllibext"
        then
          AC_MSG_RESULT(found $tcllibpath/lib$tcllibfns$tcllibext)
          TCLLIB="$tcllibpath"
          TCLLIBFN="$tcllibfns$tcllibext"
          TCLLIBEXT="$tcllibext"
          TCLLIBFNS="$tcllibfns"
          break 3
        fi
      done
    done
  done
fi

# Show if $TCLLIBFN wasn't found
if test "${TCLLIBFN-x}" = "x"
then
  AC_MSG_RESULT(not found)
fi
AC_SUBST(TCLLIB)dnl
AC_SUBST(TCLLIBFN)dnl
])dnl


dnl  EGG_TCL_CHECK_HEADER()
dnl
AC_DEFUN(EGG_TCL_CHECK_HEADER, [dnl
AC_MSG_CHECKING(for Tcl header)

# Attempt autodetect for $TCLINCFN if it's not set
if test ! "${TCLINCFN-x}" = "x"
then
  AC_MSG_RESULT(using $TCLINC/$TCLINCFN)
else
  for tclheaderpath in $tclheaderpaths
  do
    for tclheaderfn in $tclheadernames
    do
      if test -r "$tclheaderpath/$tclheaderfn"
      then
        AC_MSG_RESULT(found $tclheaderpath/$tclheaderfn)
        TCLINC="$tclheaderpath"
        TCLINCFN="$tclheaderfn"
        break 2
      fi
    done
  done
  # FreeBSD hack ...
  if test "${TCLINCFN-x}" = "x"
  then
    for tcllibfns in $tcllibnames
    do
      for tclheaderpath in $tclheaderpaths
      do
        for tclheaderfn in $tclheadernames
        do
          if test -r "$tclheaderpath/$tcllibfns/$tclheaderfn"
          then
            AC_MSG_RESULT(found $tclheaderpath/$tcllibfns/$tclheaderfn)
            TCLINC="$tclheaderpath/$tcllibfns"
            TCLINCFN="$tclheaderfn"
            break 3
          fi
        done
      done
    done
  fi
fi

# Show if $TCLINCFN wasn't found
if test "${TCLINCFN-x}" = "x"
then
  AC_MSG_RESULT(not found)
fi
AC_SUBST(TCLINC)dnl
AC_SUBST(TCLINCFN)dnl
])dnl


dnl  EGG_CACHE_UNSET(CACHE-ID)
dnl
dnl  Unsets a certain cache item. Typically called before using
dnl  the AC_CACHE_*() macros.
AC_DEFUN(EGG_CACHE_UNSET, [dnl
  unset $1
])


dnl  EGG_TCL_DETECT_CHANGE()
dnl
dnl  Detect whether the Tcl system has changed since our last
dnl  configure run. Set egg_tcl_changed accordingly.
dnl
dnl  Tcl related feature and version checks should re-run their
dnl  checks as soon as egg_tcl_changed is set to "yes".
AC_DEFUN(EGG_TCL_DETECT_CHANGE, [dnl
  AC_MSG_CHECKING(whether the Tcl system has changed)
  egg_tcl_changed=yes
  egg_tcl_id="$TCLLIB:$TCLLIBFN:$TCLINC:$TCLINCFN"
  if test ! "$egg_tcl_id" = ":::"
  then
    egg_tcl_cached=yes
    AC_CACHE_VAL(egg_cv_var_tcl_id, [dnl
      egg_cv_var_tcl_id="$egg_tcl_id"
      egg_tcl_cached=no
    ])
    if test "$egg_tcl_cached" = "yes"
    then
      if test "${egg_cv_var_tcl_id-x}" = "${egg_tcl_id-x}"
      then
        egg_tcl_changed=no
      else
        egg_cv_var_tcl_id="$egg_tcl_id"
      fi
    fi
  fi
  if test "$egg_tcl_changed" = "yes"
  then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi
])


dnl  EGG_TCL_CHECK_VERSION()
dnl
AC_DEFUN(EGG_TCL_CHECK_VERSION, [dnl
# Both TCLLIBFN & TCLINCFN must be set, or we bail
TCL_FOUND=0
if test ! "${TCLLIBFN-x}" = "x" && test ! "${TCLINCFN-x}" = "x"
then
  TCL_FOUND=1

  # Check Tcl's version
  if test "$egg_tcl_changed" = "yes"
  then
    EGG_CACHE_UNSET(egg_cv_var_tcl_version)
  fi
  AC_MSG_CHECKING(for Tcl version)
  AC_CACHE_VAL(egg_cv_var_tcl_version, [dnl
    egg_cv_var_tcl_version=`grep TCL_VERSION $TCLINC/$TCLINCFN | $HEAD_1 | $AWK '{gsub(/\"/, "", [$]3); print [$]3}'`
  ])

  if test ! "${egg_cv_var_tcl_version-x}" = "x"
  then
    AC_MSG_RESULT($egg_cv_var_tcl_version)
  else
    AC_MSG_RESULT(not found)
    TCL_FOUND=0
  fi

  # Check Tcl's patch level (if available)
  if test "$egg_tcl_changed" = "yes"
  then
    EGG_CACHE_UNSET(egg_cv_var_tcl_patch_level)
  fi
  AC_MSG_CHECKING(for Tcl patch level)
  AC_CACHE_VAL(egg_cv_var_tcl_patch_level, [dnl
    eval "egg_cv_var_tcl_patch_level=`grep TCL_PATCH_LEVEL $TCLINC/$TCLINCFN | $HEAD_1 | $AWK '{gsub(/\"/, "", [$]3); print [$]3}'`"
  ])

  if test ! "${egg_cv_var_tcl_patch_level-x}" = "x"
  then
    AC_MSG_RESULT($egg_cv_var_tcl_patch_level)
  else
    egg_cv_var_tcl_patch_level="unknown"
    AC_MSG_RESULT(unknown)
  fi
fi

# Check if we found Tcl's version
if test "$TCL_FOUND" = 0
then
  cat << 'EOF' >&2
configure: error:

  I can't find Tcl on this system.

  The download website is at:
        http://www.tcl.tk/software/tcltk/download84.html
  A nice direct link for the tarball is:
        http://aleron.dl.sourceforge.net/sourceforge/tcl/tcl8.4.4-src.tar.gz

  OR

  Just type: ./build -T

EOF
  exit 1
fi
])dnl


dnl  EGG_TCL_CHECK_PRE80()
dnl
AC_DEFUN(EGG_TCL_CHECK_PRE80, [dnl
# Is this version of Tcl too old for us to use ?
TCL_VER_PRE80=`echo $egg_cv_var_tcl_version | $AWK '{split([$]1, i, "."); if (i[[1]] < 8) print "yes"; else print "no"}'`
if test "$TCL_VER_PRE80" = "yes"
then
  cat << EOF >&2
configure: error:

  Your Tcl version is much too old for Eggdrop to use.
  I suggest you download and compile a more recent version.
  The most reliable current version is $tclrecommendver and
  can be downloaded from $tclrecommendsite

EOF
  exit 1
fi
])dnl


dnl  EGG_TCL_TESTLIBS()
dnl
AC_DEFUN(EGG_TCL_TESTLIBS, [dnl
# Set variables for Tcl library tests
TCL_TEST_LIB="$TCLLIBFNS"
TCL_TEST_OTHERLIBS="-L$TCLLIB $EGG_MATH_LIB"

if test ! "${ac_cv_lib_pthread-x}" = "x"
then
  TCL_TEST_OTHERLIBS="$TCL_TEST_OTHERLIBS $ac_cv_lib_pthread"
fi
])dnl


dnl  EGG_TCL_CHECK_FREE()
dnl
AC_DEFUN(EGG_TCL_CHECK_FREE, [dnl
if test "$egg_tcl_changed" = "yes"
then
  EGG_CACHE_UNSET(egg_cv_var_tcl_free)
fi

# Check for Tcl_Free()
AC_CHECK_LIB($TCL_TEST_LIB, Tcl_Free, egg_cv_var_tcl_free="yes", egg_cv_var_tcl_free="no", $TCL_TEST_OTHERLIBS)

if test "$egg_cv_var_tcl_free" = "yes"
then
  AC_DEFINE(HAVE_TCL_FREE, 1, [Define for Tcl that has Tcl_Free() (7.5p1 and later)])dnl
fi
])dnl


dnl  EGG_TCL_CHECK_THREADS()
dnl
AC_DEFUN(EGG_TCL_CHECK_THREADS, [dnl
if test "$egg_tcl_changed" = "yes"
then
  EGG_CACHE_UNSET(egg_cv_var_tcl_threaded)
fi

# Check for TclpFinalizeThreadData()
AC_CHECK_LIB($TCL_TEST_LIB, TclpFinalizeThreadData, egg_cv_var_tcl_threaded="yes", egg_cv_var_tcl_threaded="no", $TCL_TEST_OTHERLIBS)
enable_tcl_threads=no
if test "$egg_cv_var_tcl_threaded" = "yes"
then
  if test "$enable_tcl_threads" = "no"
  then

    cat << 'EOF' >&2
configure: warning:

  You have disabled threads support on a system with a threaded Tcl library.
  Tcl features that rely on scheduled events may not function properly.

EOF
  fi

  # Add pthread library to $LIBS if we need it
  if test ! "${ac_cv_lib_pthread-x}" = "x"
  then
    LIBS="$ac_cv_lib_pthread $LIBS"
  fi
fi
])dnl


dnl  EGG_TCL_LIB_REQS()
dnl
AC_DEFUN(EGG_TCL_LIB_REQS, [dnl
if test "$EGG_CYGWIN" = "yes"
then
  TCL_REQS="$TCLLIB/lib$TCLLIBFN"
  TCL_LIBS="-L$TCLLIB -l$TCLLIBFNS $EGG_MATH_LIB"
else
 if test ! "$TCLLIBEXT" = ".a"
 then
    cat << 'EOF' >&2
configure: warning:

  Your Tcl library is not compiled statically.
  You will need to compile Tcl statically to successfully compile wraith.

EOF
  exit 1
 else
  # Was the --with-tcllib option given ?
  if test ! "${tcllibname-x}" = "x"
  then
    TCL_REQS="$TCLLIB/lib$TCLLIBFN"
    TCL_LIBS="$TCLLIB/lib$TCLLIBFN $EGG_MATH_LIB"
  else
    TCL_REQS="$TCLLIB/lib$TCLLIBFN"
    TCL_LIBS="-L$TCLLIB -l$TCLLIBFNS $EGG_MATH_LIB"
  fi
 fi
fi
AC_SUBST(TCL_REQS)dnl
AC_SUBST(TCL_LIBS)dnl
])dnl


dnl  EGG_SUBST_VERSION()
dnl
AC_DEFUN(EGG_SUBST_VERSION, [dnl
VERSION=`grep "char" $srcdir/src/main.c | $AWK '/egg_version/ {print [$]5}' | sed -e 's/\"//g' | sed -e 's/\;//g'`
if ! test -f $srcdir/pack/pack.cfg; then
 cat << EOF >&2
configure: error:

  Your pack cfg is missing, please copy it to $srcdir/pack/pack.cfg

EOF
 exit 1
fi
PACKNAME=`grep "PACKNAME " $srcdir/pack/pack.cfg | $AWK '/PACKNAME/ {print [$]2}'`
version_num=`echo $VERSION | $AWK 'BEGIN {FS = "."} {printf("%d%02d%02d", [$]1, [$]2, [$]3)}'`
AC_DEFINE_UNQUOTED(EGG_VERSION, $version_num, [Defines the current pack version])dnl
AC_SUBST(VERSION)dnl
AC_SUBST(NUMVER)dnl
AC_SUBST(PACKNAME)dnl
])dnl


dnl  EGG_SUBST_MOD_UPDIR()
dnl
dnl  Since module's Makefiles aren't generated by configure, some
dnl  paths in src/mod/Makefile.in take care of them. For correct
dnl  path "calculation", we need to keep absolute paths in mind
dnl  (which don't need a "../" pre-pended).
AC_DEFUN(EGG_SUBST_MOD_UPDIR, [dnl
case "$srcdir" in
  [[\\/]]* | ?:[[\\/]]*)
    MOD_UPDIR=""
  ;;
  *)
    MOD_UPDIR="../"
  ;;
esac
AC_SUBST(MOD_UPDIR)dnl
])dnl


dnl EGG_REPLACE_IF_CHANGED(FILE-NAME, CONTENTS-CMDS, INIT-CMDS)
dnl
dnl Replace FILE-NAME if the newly created contents differs from the existing
dnl file contents. Otherwise, leave the file alone. This avoids needless
dnl recompiles.
m4_define(EGG_REPLACE_IF_CHANGED,
[
  AC_CONFIG_COMMANDS([replace-if-changed],
  [[
    egg_replace_file="$1"
    $2
    if test -f "$egg_replace_file" && cmp -s conftest.out $egg_replace_file; then
      echo "$1 is unchanged"
    else
      echo "creating $1"
      mv conftest.out $egg_replace_file
    fi
    rm -f conftest.out
  ]],
  [[$3]])
])

dnl EGG_TCL_LUSH()
AC_DEFUN([EGG_TCL_LUSH],
[
  EGG_REPLACE_IF_CHANGED(lush.h,
  [
    cat > conftest.out << EOF

/* Ignore me but do not erase me. I am a kludge. */

#include "${egg_tclinc}/${egg_tclincfn}"

EOF
  ], [
    egg_tclinc="$TCLINC"
    egg_tclincfn="$TCLINCFN"
  ])
])


dnl  EGG_SAVE_PARAMETERS()
dnl
AC_DEFUN(EGG_SAVE_PARAMETERS, [dnl
  # Remove --cache-file and --srcdir arguments so they do not pile up.
  egg_ac_parameters=
  ac_prev=
  for ac_arg in $ac_configure_args; do
    if test -n "$ac_prev"; then
      ac_prev=
      continue
    fi
    case $ac_arg in
    -cache-file | --cache-file | --cache-fil | --cache-fi \
    | --cache-f | --cache- | --cache | --cach | --cac | --ca | --c)
      ac_prev=cache_file ;;
    -cache-file=* | --cache-file=* | --cache-fil=* | --cache-fi=* \
    | --cache-f=* | --cache-=* | --cache=* | --cach=* | --cac=* | --ca=* \
    | --c=*)
      ;;
    --config-cache | -C)
      ;;
    -srcdir | --srcdir | --srcdi | --srcd | --src | --sr)
      ac_prev=srcdir ;;
    -srcdir=* | --srcdir=* | --srcdi=* | --srcd=* | --src=* | --sr=*)
      ;;
    *) egg_ac_parameters="$egg_ac_parameters $ac_arg" ;;
    esac
  done

  AC_SUBST(egg_ac_parameters)dnl
])dnl


AC_DEFUN([AC_PROG_CC_WIN32], [
AC_MSG_CHECKING([how to access the Win32 API])
WIN32FLAGS=
AC_TRY_COMPILE(,[
#ifndef WIN32
# ifndef _WIN32
#  error WIN32 or _WIN32 not defined
# endif
#endif], [
dnl found windows.h with the current config.
AC_MSG_RESULT([present by default])
], [
dnl try -mwin32
ac_compile_save="$ac_compile"
dnl we change CC so config.log looks correct
save_CC="$CC"
ac_compile="$ac_compile -mwin32"
CC="$CC -mwin32"
AC_TRY_COMPILE(,[
#ifndef WIN32
# ifndef _WIN32
#  error WIN32 or _WIN32 not defined
# endif
#endif], [
dnl found windows.h using -mwin32
AC_MSG_RESULT([found via -mwin32])
ac_compile="$ac_compile_save"
CC="$save_CC"
WIN32FLAGS="-mwin32"
], [
ac_compile="$ac_compile_save"
CC="$save_CC"
AC_MSG_RESULT([not found])
])
])

])
dnl
