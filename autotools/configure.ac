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
AC_PROG_CC
EGG_CHECK_CC

# Crazy machines
AC_AIX
AC_ISC_POSIX
AC_MINIX

#checkpoint
AC_CACHE_SAVE 

# Speedup compile
EGG_CHECK_CCPIPE

EGG_CHECK_DEPMODE

# Create static binaries
EGG_CHECK_CCSTATIC
AC_SUBST(CCDEBUG)dnl

# Checks for programs
AC_PROG_MAKE_SET
EGG_PROG_HEAD_1
EGG_PROG_STRIP
EGG_PROG_AWK
EGG_PROG_BASENAME
EGG_PROG_OBJCOPY
#EGG_PROG_CCACHE
EGG_PROG_DISTCC
AC_CHECK_PROG(UNAME,uname,uname)

# Test the os and set the module linking settings
EGG_CHECK_OS
EGG_CHECK_DNS

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
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h locale.h netdb.h netinet/in.h])
AC_CHECK_HEADERS([sys/file.h sys/ioctl.h sys/param.h sys/socket.h wchar.h])

#checkpoint
AC_CACHE_SAVE
AC_CHECK_HEADERS(openssl/ssl.h openssl/crypto.h)
AC_CHECK_HEADERS(zlib.h)
EGG_CHECK_ZLIB
EGG_CHECK_SSL


AC_SUBST(ZLIB)dnl
AC_SUBST(SSL)dnl



# Checks for typedefs, structures, and compiler characteristics
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T

AC_C_CONST
AC_C_BIGENDIAN
AC_C_INLINE
AC_CHECK_SIZEOF(long, 0)
AC_CHECK_SIZEOF(int, 0)


#checkpoint
AC_CACHE_SAVE 

AC_C_VOLATILE
AC_PROG_GCC_TRADITIONAL
AC_STRUCT_TIMEZONE
AC_STRUCT_TM

#checkpoint
AC_CACHE_SAVE 

# Check for IPv6 support
#EGG_IPV6_SUPPORTED
EGG_IPV6_OPTIONS
EGG_CHECK_SOCKLEN_T

# Checks for library functions

#checkpoint
AC_CACHE_SAVE

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_STRFTIME

#checkpoint
AC_CACHE_SAVE
AC_CHECK_FUNCS([clock dprintf fsync getdtablesize getrusage inet_aton isascii gethostbyname2])
AC_CHECK_FUNCS([getipnodebyname memcpy memset random rename setpgid sigaction sigemptyset snprintf srandom])
#checkpoint
AC_CACHE_SAVE
AC_CHECK_FUNCS([strcasecmp strncasecmp vsnprintf])

#autoscan suggested this....
#These are recommended by autoscan, but no code supports it currently
#AC_CHECK_FUNCS([alarm bzero dup2 getcwd gethostbyaddr gethostbyname gethostname getpass gettimeofday inet_ntoa mbrlen mkdir munmap realpath select setlocale socket strchr strerror strpbrk strrchr strstr strtol tzset])
EGG_CHECK_FUNC_VSPRINTF
EGG_CHECK_FUNC_UNAME

# Make sure we have stdc headers, since we can't compile without them
EGG_HEADER_STDC

#checkpoint
AC_CACHE_SAVE 

# Where is Tcl?  Is it here?
# ---------- begin robey's Tcl thingies
# (well, what used to be robey's Tcl thingies...)

# Latest tested Tcl version to recommend if Tcl isn't found
tclrecommendver="8.4"

# Site recommended to download Tcl from
tclrecommendsite="http://www.tcl.tk/software/tcltk/download84.html"

# Tcl library filename prefixes (also used for Tcl header dir on FreeBSD)
tcllibnames="tcl tcl8.4 tcl84 tcl8.3 tcl83 tcl8.2 tcl82 tcl8.1 tcl81 \
	tcl8.0 tcl80"

# Tcl library filename suffixes
tcllibextensions=".a .so .so.1 .so.1.0 .so.1.2 .sl .dll .dylib"

# Tcl library search paths
tcllibpaths="${HOME}/lib ${HOME}/tcl/lib ${HOME} \
        /usr/local/lib /usr/local/pkgs/tcl/lib \
	/usr/lib /lib /usr/i486-linuxaout/lib \
	/beos/system/lib /sys/lib"

# Tcl header filenames
tclheadernames="tcl.h"

# Tcl header search paths
tclheaderpaths="${HOME}/include ${HOME}/tcl/include ${HOME} \
        /usr/local/include /usr/local/pkgs/tcl/include \
	/usr/include /beos/system/include /beos/devel/include \
       /sys/include"

EGG_TCL_ARG_WITH
EGG_TCL_ENV
EGG_TCL_WITH_TCLLIB
EGG_TCL_WITH_TCLINC
EGG_TCL_FIND_LIBRARY
EGG_TCL_FIND_HEADER
EGG_TCL_CHECK_LIBRARY
EGG_TCL_CHECK_HEADER
EGG_TCL_DETECT_CHANGE

#checkpoint
AC_CACHE_SAVE 

EGG_TCL_CHECK_VERSION
EGG_TCL_CHECK_PRE80
EGG_TCL_TESTLIBS
EGG_TCL_CHECK_FREE
EGG_TCL_CHECK_THREADS
EGG_TCL_LIB_REQS
EGG_TCL_LUSH

#checkpoint
AC_CACHE_SAVE 
# ---------- end of (what used to be) robey's Tcl thingies

EGG_SUBST_VERSION
DO_PACK_CFG
EGG_SUBST_MOD_UPDIR
DO_DEPS

AC_CONFIG_FILES([Makefile src/Makefile src/compat/Makefile src/mod/Makefile src/mod/dns.mod/Makefile])
AC_OUTPUT
