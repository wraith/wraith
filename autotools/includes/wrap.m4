dnl  FIND_WRAPS()
dnl
AC_DEFUN(FIND_WRAPS, [dnl

ld_line="-Wl"
for func in `grep -hrsI __wrap_ src/compat/* | sed -e '[s/^__wrap_\(.*\)(.*/\1/]'`; do

 ld_line="${ld_line},--wrap,${func}";
# echo "FUNC: $func :: $ld_line";
 WRAP="$ld_line"
 AC_SUBST(WRAP)
done

])dnl
