# DO_DEPS
# ------------------------------
AC_DEFUN([DO_DEPS],
[
files="src/Makefile.in src/compat/Makefile.in src/mod/channels.mod/Makefile src/mod/compress.mod/Makefile src/mod/console.mod/Makefile src/mod/ctcp.mod/Makefile src/mod/dns.mod/Makefile.in src/mod/irc.mod/Makefile src/mod/notes.mod/Makefile src/mod/server.mod/Makefile src/mod/share.mod/Makefile src/mod/transfer.mod/Makefile src/mod/update.mod/Makefile"
for mf in $files; do
  # Strip MF so we end up with the name of the file.
#  echo "MF: $mf"
  mf=`echo "$mf" | sed -e 's/:.*$//'`
  dirpart=`AS_DIRNAME("$mf")`
#  echo "dirpart: $dirpart mf: $mf"
  rm -rf "$dirpart/.deps/"
  test -d "$dirpart/.deps" || mkdir "$dirpart/.deps"
  for file in `sed -n -e '
    /^OBJS = .*\\\\$/ {
      s/^OBJS = //
      :loop   s/\\\\$//
        p
        n
        /\\\\$/ b loop
      p
    }
    /^OBJS = / s/^OBJS = //p' < "$mf"`;
  do
    base=`basename $file .o`
    test -f "$dirpart/$base.c" || continue
#    fdir=`AS_DIRNAME(["$file"])`
#    AS_MKDIR_P([$dirpart/$fdir])
#    echo "creating $dirpart/.deps/$base.Po"
    echo '# dummy' > "$dirpart/.deps/$base.Po"
    echo "include .deps/$base.Po" >> "$dirpart/.deps/includes"
    echo "_$base.c:" >> "$dirpart/.deps/includes"
  done
done
])# DO_DEPS


