dnl EGG_CHECK_PACKCFG()
AC_DEFUN([EGG_CHECK_PACKCFG],
[
  cfg="pack/pack.cfg"
  AC_ARG_WITH(cfg,
              [  --with-cfg=PATH      full path to pack.cfg],
              [cfg="$withval"])
])

dnl  DO_PACK_CFG()
dnl
AC_DEFUN(DO_PACK_CFG, [dnl

CFG="$cfg"
AC_SUBST(CFG)


#for define in `sed -n -e '[s/^+ \([^ ]*\) .*/\1/ p]' $cfg`; do
# u_define="S_$define"
# AC_DEFINE_UNQUOTED($u_define)
#done

])dnl

