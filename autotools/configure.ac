dnl Process this file with autoconf to produce a configure script.
dnl
dnl

AC_PREREQ(2.59)
AC_INIT([wraith],[],[wraith@shatow.net])
AC_CONFIG_SRCDIR(src/eggdrop.h)
AC_CONFIG_HEADER(config.h)
AC_COPYRIGHT([Copyright (c) 2003, 2004 Bryan Drewery (bryan)])
AC_REVISION($Revision$)

EGG_SAVE_PARAMETERS

# Setup build environment
dnl Search for newer gcc versions before older
#AC_REQUIRE_CPP
AC_LANG(C++)
AC_PROG_CXX([g++-3.3 g++33 g++-3 g++3 g++ c++ gpp aCC CC cxx cc++ cl FCC KCC RCC xlC_r xlC])
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
AC_CHECK_HEADERS([sys/file.h sys/ioctl.h sys/param.h sys/socket.h wchar.h sys/ptrace.h paths.h])

#checkpoint
AC_CACHE_SAVE
#AC_CHECK_HEADERS(openssl/ssl.h openssl/crypto.h)
#AC_CHECK_HEADERS(zlib.h)
#EGG_CHECK_ZLIB
#EGG_CHECK_SSL


AC_SUBST(ZLIB)dnl
#AC_SUBST(SSL)dnl

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

AC_CHECK_SIZEOF(int, 0)
AC_CHECK_SIZEOF(short, 0)
AC_CHECK_SIZEOF(long, 0)
AC_CHECK_SIZEOF(long long,0)


AC_STRUCT_TIMEZONE
AC_STRUCT_TM

#checkpoint
AC_CACHE_SAVE 

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
AC_CHECK_FUNCS([clock fsync getrusage isascii inet_aton])
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

AC_CONFIG_FILES([Makefile src/Makefile src/compat/Makefile src/crypto/Makefile src/mod/Makefile])
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
])

