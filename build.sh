#! /bin/sh
# We want to use BASH, not whatever /bin/sh points to.
if test -z "$BASH"; then
  PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
  bash="`which bash`"
  ${bash} $0 ${1+"$@"}
  exit 0
fi


PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin:${HOME}/bin
# Prefer gawk
AWK="`which awk`"
if test -z "${AWK}"; then
 AWK="`which gawk`"
fi

if [ -d .git ]; then
  BUILDTS=$(git log -1 --pretty=format:%ct HEAD)
  ver=$(git describe)
else
  ver=$($AWK '/^VERSION/ {print $3}' Makefile.in)
  BUILDTS=`grep -m 1 "BUILDTS = " Makefile.in | ${AWK} '{print $3}'`
fi

# Convert timestamp into readable format
rm -f ts > /dev/null 2>&1
gcc -o ts src/timestamp.c > /dev/null 2>&1
builddate=`./ts ${BUILDTS}`
rm -f ts > /dev/null 2>&1

#Display banner
clear
head -n 8 README

echo -e "Version:   ${ver}\nBuild:     ${builddate}"
echo ""

usage()
{
    echo "Usage: $0 [-bcCdnP] pack.cfg"
    echo
    echo "    The options are as follows:"
    echo "    -b        Use bzip2 instead of gzip when packaging."
    echo "    -c        Cleans up old binaries/files before compile."
    echo "    -C        Preforms a distclean before making."
    echo "    -d        Builds a debug package."
    echo "    -n        Do not package the binaries."
    echo "    -P        For development (Don't compile/rm binaries)"
}

debug=0
clean=0
nopkg=0
bzip=0
pkg=0

while getopts bCcdhnP: opt ; do
        case "$opt" in
        b) bzip=1 ;;
	c) clean=1 ;;
	C) clean=2 ;;
        d) debug=1 ;;
	h) usage; exit 0 ;;
        n) nopkg=1 ;;
        P) pkg=1 ;;
        *) usage; exit 1 ;;

        esac
done

shift $(($OPTIND - 1))

pack=$1
if ! [ -f $pack ]; then
  echo "Cannot read $pack" >&2
  exit 1
fi

if test -z "$1"; then
 usage
 exit 1
fi


PACKNAME=`grep "PACKNAME " ${pack} | $AWK '/PACKNAME/ {print $2}'`

rm=1
compile=1
if [ $pkg = "1" ]; then
 rm=0
 compile=0
fi

if [ $debug = "1" ]; then
 dmake="d"
 d="-debug"
else
 dmake=""
 d=""
fi

# Figure what bins we're making
case `uname` in
  Linux) os=Linux;;
  FreeBSD) os=FreeBSD;;
#  FreeBSD) case `uname -r` in
#    5*) os=FreeBSD5;;
#    4*) os=FreeBSD4;;
#    3*) os=FreeBSD3;;
#  esac;;
  OpenBSD) os=OpenBSD;;
  NetBSD) os=NetBSD;;
  SunOS) os=Solaris;;
esac

if test -z $os
then
  echo "[!] Automated packaging disabled, `uname` isn't recognized"
fi

if [ $compile = "1" ]; then

 echo "[*] Building ${PACKNAME} for $os"

 MAKE="`which gmake`"
 if test -z "${MAKE}"; then
  MAKE="`which make`"
 fi

 if [ $clean = "2" ]; then
  if test -f Makefile; then
   echo "[*] DistCleaning old files..."
   ${MAKE} distclean > /dev/null
  fi
 fi

 # Run ./configure, then verify it's ok
 echo "[*] Configuring..."
 umask 077 >/dev/null

 ./configure --silent

 if [ $clean = "1" ]; then
  echo "[*] Cleaning up old binaries/files..."
  ${MAKE} clean > /dev/null
 fi
fi

_build()
{
 if [ $compile = "1" ]; then
  echo "[*] Building ${dmake}${tb}..."
  ${MAKE} ${dmake}${tb}
  if ! test -f ${tb}; then
    echo "[!] ${dmake}${tb} build failed"
    exit 1
  fi
 fi
 if [ $nopkg = "0" -o $pkg = "1" ]; then
  echo "[*] Hashing and initializing settings in binary"
   cp ${tb} ${tb}.$os-$ver${d} > /dev/null 2>&1
  ./${tb}.$os-$ver${d} -q ${pack}
  rm=1
 elif [ $nopkg = "0" ]; then
   mv ${tb} ${tb}.$os-$ver${d} > /dev/null 2>&1
 fi
}

tb="wraith"
_build

if [ $bzip = "1" ]; then
  zip="j"
  ext="bz2"
else
  zip="z"
  ext="gz"
fi

# Wrap it nicely up into an archive
if [ $nopkg = "0" -o $pkg = "1" ]; then
  echo "[*] Packaging..."
  tar -c${zip}f ${PACKNAME}.$os-$ver${d}.tar.${ext} wraith.$os-$ver${d}
  chmod 0644 ${PACKNAME}.$os-$ver${d}.tar.${ext}
  if [ $rm = "1" ]; then
    rm -f *$os-$ver${d}
  fi
  echo "Binaries are now in '${PACKNAME}.$os-$ver${d}.tar.${ext}'."
fi
