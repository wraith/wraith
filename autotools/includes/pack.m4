dnl  DO_PACK_CFG()
dnl
AC_DEFUN(DO_PACK_CFG, [dnl

for define in `sed -n -e '[s/^+ \([^ ]*\) .*/\1/ p]' pack/pack.cfg`; do
 u_define="S_$define"
 AC_DEFINE_UNQUOTED($u_define)
done

])dnl

