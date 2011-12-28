dnl Process this file with autoconf to produce a configure script.
dnl
dnl

AC_PREREQ(2.59)
AC_INIT([wraith],[],[wraith@botpack.net])
AC_CONFIG_SRCDIR(src/eggdrop.h)
AC_CONFIG_HEADER(config.h)
AC_COPYRIGHT([Copyright (c) 2003 - 2010 Bryan Drewery])
AC_REVISION($Revision$)

EGG_SAVE_PARAMETERS

# Setup build environment
dnl Search for newer gcc versions before older
#AC_REQUIRE_CPP
AC_LANG(C++)
AC_PROG_CXX([g++45 g++-4.5 g++-4.4 g++-4.4.1 g++-4.4.2 g++-4.4.0 g++44 g++4 g++-3.3 g++33 g++-3 g++3 g++ c++ gpp aCC CC cxx cc++ cl FCC KCC RCC xlC_r xlC])
dnl AC_PROG_CC([gcc34 gcc-3.4 gcc33 gcc3 gcc-3.3 gcc cc c1])
EGG_CHECK_CC

# Crazy machines
AC_AIX
AC_ISC_POSIX
AC_MINIX

#checkpoint
AC_CACHE_SAVE 

# Speedup compile
EGG_CHECK_CCWALL
EGG_CHECK_CCPIPE

EGG_CHECK_DEPMODE

#checkpoint
AC_CACHE_SAVE 

# Checks for programs
AC_PROG_MAKE_SET
AC_PROG_SED
EGG_PROG_HEAD_1
EGG_PROG_AWK
EGG_PROG_BASENAME
AC_CHECK_PROG(OBJCOPY, objcopy, [objcopy --remove-section=.note --remove-section=.comment], touch)
AC_CHECK_PROG(STRIP, strip, strip, touch)
AC_CHECK_PROG(DIFF, gdiff, gdiff, diff)
AC_CHECK_PROG(UNAME, uname, uname)
AC_CHECK_PROG(CCACHE, ccache, ccache)
AC_CHECK_PROG(DISTCC, distcc, distcc)

# Test the os and set the module linking settings
EGG_CHECK_OS

# Check for IPv6 support
#EGG_IPV6_SUPPORTED
EGG_IPV6_OPTIONS
EGG_CHECK_SOCKLEN_T

# Check for inconsistent random() functions
EGG_CHECK_RANDOM_MAX

# Create static binaries
EGG_CHECK_CCSTATIC
AC_SUBST(CCDEBUG)dnl

AC_SUBST(MAKEJOBS)dnl

#checkpoint
AC_CACHE_SAVE 

# Checks for system libraries
EGG_CHECK_LIBS

# Checks for header files
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_HEADER_TIME
AC_HEADER_STAT
#checkpoint
AC_CACHE_SAVE 
AC_CHECK_HEADERS([stdarg.h std_arg.h arpa/inet.h fcntl.h limits.h locale.h netdb.h netinet/in.h])
AC_CHECK_HEADERS([sys/file.h sys/ioctl.h sys/param.h sys/socket.h wchar.h sys/ptrace.h paths.h sys/prctl.h])

#checkpoint
AC_CACHE_SAVE
#AC_CHECK_HEADERS(openssl/ssl.h openssl/crypto.h)
#AC_CHECK_HEADERS(zlib.h)
#EGG_CHECK_ZLIB
CHECK_SSL

# TCL checks

# Tcl version to recommend if no Tcl is found, and the site where it can be
# found for download.
tclrecommendver="8.5.X"
tclrecommendsite="ftp://tcl.activestate.com/pub/tcl/tcl8_5/"

# Tcl library filename prefixes, suffixes, and search paths.
tcllibnames="tcl8.5 tcl85 tcl8.4 tcl84 tcl8.3 tcl83 tcl8.2 tcl82 \
             tcl8.1 tcl81 tcl8.0 tcl80 tcl tcl7.6 tcl76 tcl7.5 tcl75 \
             tcl7.4 tcl74 tcl7.3 tcl73 tcl7.2 tcl72 tcl7.1 tcl71 \
             tcl7.0 tcl70"

tcllibextensions=".so .so.1 .so.1.0 .so.1.1 .so.1.2 .dll .dylib .sl .a"

tcllibpaths="$HOME/lib $HOME/tcl/lib \
             /usr/local/lib /usr/lib /lib /lib64 /usr/lib64 \
             /usr/local/lib/tcl8.5 /usr/lib/tcl8.5 \
             /usr/local/lib/tcl8.4 /usr/lib/tcl8.4 \
             /usr/local/lib/tcl8.3 /usr/lib/tcl8.3 \
             /usr/local/pkgs/tcl/lib /sys/lib /usr/pkg/lib \
             /usr/i486-linuxaout/lib /beos/system/lib $HOME"

# Tcl header filenames and search paths.
tclheadernames="tcl.h"
tclheaderpaths="$HOME/include $HOME/tcl/include \
                /usr/local/include /usr/include \
                /usr/local/include/tcl8.5 /usr/include/tcl8.5 \
                /usr/local/include/tcl8.4 /usr/include/tcl8.4 \
                /usr/local/include/tcl8.3 /usr/include/tcl8.3 \
                /usr/local/pkgs/tcl/include /sys/include \
                /usr/pkg/lib /beos/system/include /beos/devel/include $HOME"


# Misc Tcl checks.
EGG_TCL_ARG_WITH
EGG_TCL_ENV
EGG_TCL_WITH_TCLLIB
EGG_TCL_WITH_TCLINC
EGG_TCL_FIND_LIBRARY
EGG_TCL_FIND_HEADER
EGG_TCL_CHECK_LIBRARY
EGG_TCL_CHECK_HEADER
EGG_TCL_DETECT_CHANGE
EGG_TCL_CHECK_VERSION
EGG_TCL_CHECK_PRE70
EGG_TCL_TESTLIBS
EGG_TCL_CHECK_FREE
#EGG_TCL_CHECK_GETCURRENTTHREAD
#EGG_TCL_CHECK_GETTHREADDATA
EGG_TCL_CHECK_SETNOTIFIER
EGG_TCL_LIB_REQS

AC_CACHE_SAVE

#AC_SUBST(ZLIB)dnl

# Checks for typedefs, structures, and compiler characteristics
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T


#AC_C_CONST
AC_C_VOLATILE
AC_C_BIGENDIAN(AC_DEFINE(B_ENDIAN, 1, [big endian]),AC_DEFINE(L_ENDIAN, 1, [little endian]))
#AC_C_INLINE
AC_C_CHAR_UNSIGNED
#AC_C_STRINGIZE
#AC_C_PROTOTYPES

#checkpoint
AC_CACHE_SAVE 

# Checks for typedefs, structures, and compiler characteristics
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_TYPE_LONG_LONG_INT
AC_TYPE_INT8_T
AC_TYPE_UINT8_T
AC_TYPE_INT16_T
AC_TYPE_UINT16_T
AC_TYPE_INT32_T
AC_TYPE_UINT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT64_T


AC_STRUCT_TIMEZONE
AC_STRUCT_TM

# Checks for library functions
AC_FUNC_FORK
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_STAT
EGG_FUNC_TIMESPEC
AC_FUNC_STRFTIME

#checkpoint
AC_CACHE_SAVE
AC_CHECK_FUNCS([clock fsync getrusage isascii getpassphrase])
AC_CHECK_FUNCS([memcpy memset random rename snprintf srandom gettime])
#checkpoint
AC_CACHE_SAVE
AC_CHECK_FUNCS([strcasecmp strncasecmp vsnprintf])

#autoscan suggested this....
#These are recommended by autoscan, but no code supports it currently
#AC_CHECK_FUNCS([alarm bzero dup2 getcwd gethostbyaddr gethostbyname gethostname getpass gettimeofday mbrlen
#mkdir munmap realpath select setlocale socket strchr strerror strpbrk strrchr strstr strtol tzset])

EGG_CHECK_FUNC_VSPRINTF
EGG_CHECK_FUNC_UNAME

# Make sure we have stdc headers, since we can't compile without them
EGG_HEADER_STDC

#checkpoint
AC_CACHE_SAVE 

EGG_SUBST_VERSION
#FIND_WRAPS
EGG_SUBST_MOD_UPDIR
DO_DEPS

AC_CONFIG_FILES([Makefile lib/Makefile src/Makefile src/compat/Makefile src/crypto/Makefile src/mod/Makefile src/buildinfo.h])
AC_OUTPUT

AH_TEMPLATE(HAVE_ST_MTIM, [Define to 1 if your struct stat has an st_mtim member])
AH_TEMPLATE(HAVE_ST_MTIMESPEC, [Define to 1 if your struct stat has an st_mtimespec member])
AH_TEMPLATE(HAVE_TIMESPEC, [Define to 1 if you have struct timespec in sys/time.h])
AH_TEMPLATE(HAVE_TIMESPECSUB2, [Define to 1 if you have a timespecsub macro or function that takes two arguments (not three)])
AH_BOTTOM([/*
 * Macros to pull sec and nsec parts of mtime from struct stat.
 */
#ifdef HAVE_ST_MTIM
# define mtim_getsec(_x)        ((_x).st_mtim.tv_sec)
# define mtim_getnsec(_x)       ((_x).st_mtim.tv_nsec)
#else
# ifdef HAVE_ST_MTIMESPEC
#  define mtim_getsec(_x)       ((_x).st_mtimespec.tv_sec)
#  define mtim_getnsec(_x)      ((_x).st_mtimespec.tv_nsec)
# else
#  define mtim_getsec(_x)       ((_x).st_mtime)
#  define mtim_getnsec(_x)      (0)
# endif /* HAVE_ST_MTIMESPEC */
#endif /* HAVE_ST_MTIM */

/*
 * Enable IPv6 debugging?
 */
#define DEBUG_IPV6 1
#define HAVE_IPV6 1

/* IPv6 sanity checks. */
#ifdef USE_IPV6
#  ifndef HAVE_IPV6
#    undef USE_IPV6
#  endif
#  ifndef HAVE_GETHOSTBYNAME2
#    ifndef HAVE_GETIPNODEBYNAME
#      undef USE_IPV6
#    endif
#  endif
#endif
])

[
GIT_REQUIRED=1
if [ "$GIT_REQUIRED" = "1" ]; then
  if ! test -d .git; then
    echo ""
    echo ""
    echo "You have downloaded a GIT version of wraith."
    echo "Either 'git clone' from a git repository or download a source tarball"
    echo "A distribution version can be found at http://wraith.botpack.net/wiki/Download"
    echo ""
    echo ""
    exit 1
  fi
fi
]

CXX="$CXX" TCLINC="$TCLINC" SSL_INCLUDES="$SSL_INCLUDES" SED="$SED" src/generate_defs.sh
[
if [ $? -ne 0 ]; then
  exit 1
fi
]

echo ""
echo ""
echo "------------ Configuring BDLIB ------------"
[
if test -d .git; then
  git submodule init
  git submodule update
fi
]
test -f lib/bdlib/configure && cd lib/bdlib && MAKEJOBS="$MAKEJOBS" ./configure
