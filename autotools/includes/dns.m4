dnl  EGG_CHECK_DNS()
dnl
AC_DEFUN(EGG_CHECK_DNS, [dnl

dns_reslib_avail="true"
AC_CHECK_FUNC(res_init, ,
  AC_CHECK_LIB(resolv, res_init, RESLIB="-lresolv",
    AC_CHECK_LIB(bind, res_init, RESLIB="-lbind", [dnl
      dns_reslib_avail="false";
    ])dnl
  )dnl
)dnl

if test "${dns_reslib_avail}" = false; then
  dns_reslib_avail="true"
  AC_CHECK_FUNC(__res_init, ,
    AC_CHECK_LIB(resolv, __res_init, RESLIB="-lresolv",
      AC_CHECK_LIB(bind, __res_init, RESLIB="-lbind", [dnl
        dns_reslib_avail="false";
      ])dnl
    )dnl
  )dnl
fi

AC_CHECK_FUNC(res_mkquery, ,
  AC_CHECK_LIB(resolv, res_mkquery, [dnl
    if test "x${RESLIB}" != "x-lresolv"; then
      RESLIB="${RESLIB} -lresolv"
    fi
  ],
    AC_CHECK_LIB(bind, res_mkquery, [dnl
      if test "x${RESLIB}" != "x-lbind"; then
        RESLIB="${RESLIB} -lbind"
      fi
    ], [dnl
      dns_reslib_avail="false";
    ])
  )
)

if test "${dns_reslib_avail}" = false; then
dns_reslib_avail="true" 
AC_CHECK_FUNC(__res_mkquery, ,
  AC_CHECK_LIB(resolv, __res_mkquery, [dnl
    if test "x${RESLIB}" != "x-lresolv"; then
      RESLIB="${RESLIB} -lresolv"
    fi
  ],
    AC_CHECK_LIB(bind, __res_mkquery, [dnl
      if test "x${RESLIB}" != "x-lbind"; then
        RESLIB="${RESLIB} -lbind"
      fi
    ], [dnl
      dns_reslib_avail="false";
    ])
  )
)
fi

if test "${dns_reslib_avail}" = false; then
  if test "$ac_cv_cygwin" = "yes"; then
    AC_MSG_CHECKING(for /usr/local/bind/lib/libbind.a)
    if test -r /usr/local/bind/lib/libbind.a; then
      AC_MSG_RESULT(yes)
      RESLIB="${RESLIB} -L/usr/local/bind/lib -lbind"
      RESINCLUDE="-I/usr/local/bind/include"
      dns_reslib_avail="true"
    else
      AC_MSG_RESULT(no)
    fi
  fi
fi

if test "${dns_reslib_avail}" = false; then
  cat >&2 <<EOF
configure: warning:

  Your system provides no functional resolver library. 
  One is required for buld.

EOF
  exit 1
fi

AC_SUBST(RESLIB)
AC_SUBST(RESINCLUDE)
])dnl


