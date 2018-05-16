# DO_DEPS
# ------------------------------


dnl  EGG_CHECK_DEPMODE()
dnl
AC_DEFUN([EGG_CHECK_DEPMODE],
[
CCDEPMODE=gcc
num=`$CXX -dumpversion | sed "s/^\\\(.\\\).*/\\\1/"`
if test $num -ge "3"; then
  CCDEPMODE=gcc3
#  GCC3="-Wpadded -Wpacked -Wno-unused-parameter -Wmissing-format-attribute -Wdisabled-optimization"
  GCC3_CFLAGS="-W -Wno-unused-parameter -Wdisabled-optimization -Wno-write-strings -Wno-format-security -Wno-format-y2k"
  GCC3_CXXFLAGS="-Woverloaded-virtual"
  GCC3DEB="-Wno-disabled-optimization -Wmissing-format-attribute"
fi
AC_SUBST(CCDEPMODE)dnl
AC_SUBST(GCC3_CFLAGS)dnl
AC_SUBST(GCC3_CXXFLAGS)dnl
AC_SUBST(GCC3DEB)dnl
AC_SUBST(GCC4DEB)dnl
])

AC_DEFUN([DO_DEPS],
[
files="src/Makefile.in"
for mf in $files; do
  # Strip MF so we end up with the name of the file.
  mf=${mf%%:*}
  dirmf=${mf%/*}
  rm -f "$dirmf/.deps/includes"
  test -d "$dirmf/.deps" || mkdir "$dirmf/.deps"
  for file in `sed -n -e '
    /^OBJS = .*\\\\$/ {
      s/^OBJS = //
      :loop   
        s/\\\\$//
        p
        n
        /\\\\$/ b loop
      p
    }
    /^OBJS = / s/^OBJS = //p' < "$mf"`;
  do
    dirpart="${dirmf}/${file}"
    dirpart="${dirpart%/*}"
    if [[ "${dirpart}" != "${dirmf}" ]]; then
      test -d "${dirpart}/.deps" || mkdir "${dirpart}/.deps"
    fi
    file="${file##*/}"
    suffix=${file##*.}
    base=${file%%.*}
    test -f "$dirpart/$base.cc" || test -f "$dirpart/$base.c" || continue
    if ! test -f "$dirpart/.deps/$base.Po"; then
      echo '# dummy' > "$dirpart/.deps/$base.Po"
      #Remove the .o file, because it needs to be recompiled for its dependancies.
      if test -f "$dirpart/${base}.${suffix}"; then
        rm -f "$dirpart/${base}.${suffix}"
      fi
    fi
    echo "include .${dirpart#${dirmf}}/.deps/$base.Po" >> "${dirmf}/.deps/includes"
  done
done
])


