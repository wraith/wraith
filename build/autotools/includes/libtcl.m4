dnl libtcl.m4
dnl   macros autoconf uses when building configure from configure.in
dnl   These are taken from Eggdrop
dnl

dnl EGG_TCL_OPTIONS()
dnl
AC_DEFUN([EGG_TCL_OPTIONS],
[
  AC_ARG_WITH(tcllib, [AS_HELP_STRING([--with-tcllib=PATH],[full path to Tcl library])], [tcllibname="$withval"])
  AC_ARG_WITH(tclinc, [AS_HELP_STRING([--with-tclinc=PATH],[full path to Tcl header])],  [tclincname="$withval"])

  MY_ARG_DISABLE([script_tcl], [TCL Script])

  WARN=0
  # Make sure either both or neither $tcllibname and $tclincname are set
  if test "x$tcllibname" != x; then
    if test "x$tclincname" = x; then
      WARN=1
      tcllibname=""
      TCLLIB=""
      TCLINC=""
    fi
  else
    if test "x$tclincname" != x; then
      WARN=1
      tclincname=""
      TCLLIB=""
      TCLINC=""
    fi
  fi

  if test "$WARN" = 1; then
    cat << 'EOF' >&2
configure: WARNING:

  You must specify both --with-tcllib and --with-tclinc for either to work.

  configure will now attempt to autodetect both the Tcl library and header.

EOF
  fi
])


dnl EGG_TCL_ENV()
dnl
AC_DEFUN([EGG_TCL_ENV],
[
  WARN=0
  # Make sure either both or neither $TCLLIB and $TCLINC are set
  if test "x$TCLLIB" != x; then
    if test "x$TCLINC" = x; then
      WARN=1
      WVAR1=TCLLIB
      WVAR2=TCLINC
      TCLLIB=""
    fi
  else
    if test "x$TCLINC" != x; then
      WARN=1
      WVAR1=TCLINC
      WVAR2=TCLLIB
      TCLINC=""
    fi
  fi

  if test "$WARN" = 1; then
    cat << EOF >&2
configure: WARNING:

  Environment variable $WVAR1 was set, but I did not detect ${WVAR2}.
  Please set both TCLLIB and TCLINC correctly if you wish to use them.

  configure will now attempt to autodetect both the Tcl library and header.

EOF
  fi
])


dnl EGG_TCL_WITH_TCLLIB()
dnl
AC_DEFUN([EGG_TCL_WITH_TCLLIB],
[
  # Look for Tcl library: if $tcllibname is set, check there first
  if test "x$tcllibname" != x; then
    if test -f "$tcllibname" && test -r "$tcllibname"; then
      TCLLIB=`echo $tcllibname | sed 's%/[[^/]][[^/]]*$%%'`
      TCLLIBFN=`$BASENAME $tcllibname | cut -c4-`
      TCLLIBEXT=".`echo $TCLLIBFN | $AWK '{j=split([$]1, i, "."); print i[[j]]}'`"
      TCLLIBFNS=`$BASENAME $tcllibname $TCLLIBEXT | cut -c4-`
    else
      cat << EOF >&2
configure: WARNING:

  The file '$tcllibname' given to option --with-tcllib is not valid.

  configure will now attempt to autodetect both the Tcl library and header.

EOF
      tcllibname=""
      tclincname=""
      TCLLIB=""
      TCLLIBFN=""
      TCLINC=""
      TCLINCFN=""
    fi
  fi
])


dnl EGG_TCL_WITH_TCLINC()
dnl
AC_DEFUN([EGG_TCL_WITH_TCLINC],
[
  # Look for Tcl header: if $tclincname is set, check there first
  if test "x$tclincname" != x; then
    if test -f "$tclincname" && test -r "$tclincname"; then
      TCLINC=`echo $tclincname | sed 's%/[[^/]][[^/]]*$%%'`
      TCLINCFN=`$BASENAME $tclincname`
    else
      cat << EOF >&2
configure: WARNING:

  The file '$tclincname' given to option --with-tclinc is not valid.

  configure will now attempt to autodetect both the Tcl library and header.

EOF
      tcllibname=""
      tclincname=""
      TCLLIB=""
      TCLLIBFN=""
      TCLINC=""
      TCLINCFN=""
    fi
  fi
])


dnl EGG_TCL_FIND_LIBRARY()
dnl
AC_DEFUN([EGG_TCL_FIND_LIBRARY],
[
  # Look for Tcl library: if $TCLLIB is set, check there first
  if test "x$TCLLIBFN" = x && test "x$TCLLIB" != x; then
    if test -d "$TCLLIB"; then
      for tcllibfns in $tcllibnames; do
        for tcllibext in $tcllibextensions; do
          if test -r "${TCLLIB}/lib${tcllibfns}${tcllibext}"; then
            TCLLIBFN="${tcllibfns}${tcllibext}"
            TCLLIBEXT="$tcllibext"
            TCLLIBFNS="$tcllibfns"
            break 2
          fi
        done
      done
    fi

    if test "x$TCLLIBFN" = x; then
      cat << 'EOF' >&2
configure: WARNING:

  Environment variable TCLLIB was set, but incorrectly.
  Please set both TCLLIB and TCLINC correctly if you wish to use them.

  configure will now attempt to autodetect both the Tcl library and header.

EOF
      TCLLIB=""
      TCLLIBFN=""
      TCLINC=""
      TCLINCFN=""
    fi
  fi
])


dnl EGG_TCL_FIND_HEADER()
dnl
AC_DEFUN([EGG_TCL_FIND_HEADER],
[
  # Look for Tcl header: if $TCLINC is set, check there first
  if test "x$TCLINCFN" = x && test "x$TCLINC" != x; then
    if test -d "$TCLINC"; then
      for tclheaderfn in $tclheadernames; do
        if test -r "${TCLINC}/${tclheaderfn}"; then
          TCLINCFN="$tclheaderfn"
          break
        fi
      done
    fi

    if test "x$TCLINCFN" = x; then
      cat << 'EOF' >&2
configure: WARNING:

  Environment variable TCLINC was set, but incorrectly.
  Please set both TCLLIB and TCLINC correctly if you wish to use them.

  configure will now attempt to autodetect both the Tcl library and header.

EOF
      TCLLIB=""
      TCLLIBFN=""
      TCLINC=""
      TCLINCFN=""
    fi
  fi
])


dnl EGG_TCL_CHECK_LIBRARY()
dnl
AC_DEFUN([EGG_TCL_CHECK_LIBRARY],
[
  if test "$enable_script_tcl" = "yes"; then
    AC_MSG_CHECKING([for Tcl library])

    # Attempt autodetect for $TCLLIBFN if it's not set
    if test "x$TCLLIBFN" != x; then
      AC_MSG_RESULT([using ${TCLLIB}/lib${TCLLIBFN}])
    else
      for tcllibfns in $tcllibnames; do
        for tcllibext in $tcllibextensions; do
          for tcllibpath in $tcllibpaths; do
            if test -r "${tcllibpath}/lib${tcllibfns}${tcllibext}"; then
              AC_MSG_RESULT([found ${tcllibpath}/lib${tcllibfns}${tcllibext}])
              TCLLIB="$tcllibpath"
              TCLLIBFN="${tcllibfns}${tcllibext}"
              TCLLIBEXT="$tcllibext"
              TCLLIBFNS="$tcllibfns"
              break 3
            fi
          done
        done
      done
    fi

    # Show if $TCLLIBFN wasn't found
    if test "x$TCLLIBFN" = x; then
      AC_MSG_RESULT([not found])
    fi
  fi

  AC_SUBST(TCLLIB)
  AC_SUBST(TCLLIBFN)
])


dnl EGG_TCL_CHECK_HEADER()
dnl
AC_DEFUN([EGG_TCL_CHECK_HEADER],
[
  if test "$enable_script_tcl" = "yes"; then
    AC_MSG_CHECKING([for Tcl header])

    # Attempt autodetect for $TCLINCFN if it's not set
    if test "x$TCLINCFN" != x; then
      AC_MSG_RESULT([using ${TCLINC}/${TCLINCFN}])
    else
      for tclheaderpath in $tclheaderpaths; do
        for tclheaderfn in $tclheadernames; do
          if test -r "${tclheaderpath}/${tclheaderfn}"; then
            AC_MSG_RESULT([found ${tclheaderpath}/${tclheaderfn}])
            TCLINC="$tclheaderpath"
            TCLINCFN="$tclheaderfn"
            break 2
          fi
        done
      done

      # FreeBSD hack ...
      if test "x$TCLINCFN" = x; then
        for tcllibfns in $tcllibnames; do
          for tclheaderpath in $tclheaderpaths; do
            for tclheaderfn in $tclheadernames; do
              if test -r "${tclheaderpath}/${tcllibfns}/${tclheaderfn}"; then
                AC_MSG_RESULT([found ${tclheaderpath}/${tcllibfns}/${tclheaderfn}])
                TCLINC="${tclheaderpath}/${tcllibfns}"
                TCLINCFN="$tclheaderfn"
                break 3
              fi
            done
          done
        done
      fi
    fi

    TCL_INCLUDES=""
    if ! test "x$TCLINC" = x; then
      TCL_INCLUDES="-I$TCLINC"
    fi

    if test "x$TCLINCFN" = x; then
      AC_MSG_RESULT([not found])
    fi
  fi

  AC_SUBST(TCL_INCLUDES)
  AC_SUBST(TCLINCFN)
])


dnl EGG_CACHE_UNSET(CACHE-ID)
dnl
dnl Unsets a certain cache item. Typically called before using the AC_CACHE_*()
dnl macros.
dnl
AC_DEFUN([EGG_CACHE_UNSET], [unset $1])


dnl EGG_TCL_DETECT_CHANGE()
dnl
dnl Detect whether the Tcl system has changed since our last configure run.
dnl Set egg_tcl_changed accordingly.
dnl
dnl Tcl related feature and version checks should re-run their checks as soon
dnl as egg_tcl_changed is set to "yes".
dnl
AC_DEFUN([EGG_TCL_DETECT_CHANGE],
[
  if test "$enable_script_tcl" = "yes"; then
    dnl NOTE: autoconf 2.50+ disables config.cache by default.
    dnl       These checks don't do us much good if cache is disabled.
    AC_MSG_CHECKING([whether the Tcl system has changed])
    egg_tcl_changed="yes"
    egg_tcl_id="${TCLLIB}:${TCLLIBFN}:${TCLINC}:${TCLINCFN}"
    if test "$egg_tcl_id" != ":::"; then
      egg_tcl_cached="yes"
      AC_CACHE_VAL(egg_cv_var_tcl_id, [
        egg_cv_var_tcl_id="$egg_tcl_id"
        egg_tcl_cached="no"
      ])
      if test "$egg_tcl_cached" = yes; then
        if test "x$egg_cv_var_tcl_id" = "x$egg_tcl_id"; then
          egg_tcl_changed="no"
        else
          egg_cv_var_tcl_id="$egg_tcl_id"
        fi
      fi
    fi

    if test "$egg_tcl_changed" = yes; then
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
    fi
  fi
])


dnl EGG_TCL_CHECK_VERSION()
dnl
AC_DEFUN([EGG_TCL_CHECK_VERSION],
[
  if test "$enable_script_tcl" = "yes"; then
    # Both TCLLIBFN & TCLINCFN must be set, or we bail
    TCL_FOUND=0
    if test "x$TCLLIBFN" != x && test "x$TCLINCFN" != x; then
      TCL_FOUND=1

      # Check Tcl's version
      if test "$egg_tcl_changed" = yes; then
        EGG_CACHE_UNSET(egg_cv_var_tcl_version)
      fi

      AC_MSG_CHECKING([for Tcl version])
      AC_CACHE_VAL(egg_cv_var_tcl_version, [
        egg_cv_var_tcl_version=`grep TCL_VERSION $TCLINC/$TCLINCFN | $HEAD_1 | $AWK '{gsub(/\"/, "", [$]3); print [$]3}'`
      ])

      if test "x$egg_cv_var_tcl_version" != x; then
        AC_MSG_RESULT([$egg_cv_var_tcl_version])
      else
        AC_MSG_RESULT([not found])
        TCL_FOUND=0
      fi

      # Check Tcl's patch level (if available)
      if test "$egg_tcl_changed" = yes; then
        EGG_CACHE_UNSET(egg_cv_var_tcl_patch_level)
      fi
      AC_MSG_CHECKING([for Tcl patch level])
      AC_CACHE_VAL(egg_cv_var_tcl_patch_level, [
        eval "egg_cv_var_tcl_patch_level=`grep TCL_PATCH_LEVEL $TCLINC/$TCLINCFN | $HEAD_1 | $AWK '{gsub(/\"/, "", [$]3); print [$]3}'`"
      ])

      if test "x$egg_cv_var_tcl_patch_level" != x; then
        AC_MSG_RESULT([$egg_cv_var_tcl_patch_level])
      else
        egg_cv_var_tcl_patch_level="unknown"
        AC_MSG_RESULT([unknown])
      fi
    fi

    # Check if we found Tcl's version
    if test "$TCL_FOUND" = 0; then
      cat << 'EOF' >&2
configure: error:

  Tcl cannot be found on this system.

  Tcl is not required. Wraith will be compiled without TCL support. If you
  already have Tcl installed on this system, please specify the path by
  rerunning ./configure using the --with-tcllib='/path/to/libtcl.so' and
  --with-tclinc='/path/to/tcl.h' options.

EOF
    enable_script_tcl="no"
    else
      AC_DEFINE(HAVE_LIBTCL, 1, [Define if you have support for libtcl])
    fi
  fi
])


dnl EGG_TCL_CHECK_PRE70()
dnl
AC_DEFUN([EGG_TCL_CHECK_PRE70],
[
  if test "$enable_script_tcl" = "yes"; then
    # Is this version of Tcl too old for us to use ?
    TCL_VER_PRE70=`echo $egg_cv_var_tcl_version | $AWK '{split([$]1, i, "."); if (i[[1]] < 7) print "yes"; else print "no"}'`
    if test "$TCL_VER_PRE70" = yes; then
      cat << EOF >&2
configure: error:

  Your Tcl version is much too old for Wraith to use. You should
  download and compile a more recent version. The most reliable
  current version is $tclrecommendver and can be downloaded from
  ${tclrecommendsite}.

  See doc/COMPILE-GUIDE's 'Tcl Detection and Installation' section
  for more information.

EOF
      exit 1
    fi
  fi
])


dnl EGG_TCL_TESTLIBS()
dnl
AC_DEFUN([EGG_TCL_TESTLIBS],
[
  # Set variables for Tcl library tests
  TCL_TEST_LIB="$TCLLIBFNS"
  TCL_TEST_OTHERLIBS="-L$TCLLIB $EGG_MATH_LIB"
  if test "x$ac_cv_lib_pthread" != x; then
    TCL_TEST_OTHERLIBS="$TCL_TEST_OTHERLIBS $ac_cv_lib_pthread"
  fi
])


dnl EGG_TCL_CHECK_FREE()
dnl
AC_DEFUN([EGG_TCL_CHECK_FREE],
[
  if test "$enable_script_tcl" = "yes"; then
    if test "$egg_tcl_changed" = yes; then
      EGG_CACHE_UNSET(egg_cv_var_tcl_free)
    fi

    # Check for Tcl_Free()
    AC_CHECK_LIB($TCL_TEST_LIB, Tcl_Free, [egg_cv_var_tcl_free="yes"], [egg_cv_var_tcl_free="no"], $TCL_TEST_OTHERLIBS)

    if test "$egg_cv_var_tcl_free" = yes; then
      AC_DEFINE(HAVE_TCL_FREE, 1, [Define for Tcl that has Tcl_Free() (7.5p1 and later).])
    fi
  fi
])


dnl EGG_TCL_CHECK_GETCURRENTTHREAD
dnl
AC_DEFUN([EGG_TCL_CHECK_GETCURRENTTHREAD],
[
  if test "$enable_script_tcl" = "yes"; then
    if test "$egg_tcl_changed" = yes; then
      EGG_CACHE_UNSET(egg_cv_var_tcl_getcurrentthread)
    fi

    # Check for Tcl_GetCurrentThread()
    AC_CHECK_LIB($TCL_TEST_LIB, Tcl_GetCurrentThread, [egg_cv_var_tcl_getcurrentthread="yes"], [egg_cv_var_tcl_getcurrentthread="no"], $TCL_TEST_OTHERLIBS)
    if test "$egg_cv_var_tcl_getcurrentthread" = yes; then
      AC_DEFINE(HAVE_TCL_GETCURRENTTHREAD, 1, [Define for Tcl that has Tcl_GetCurrentThread() (8.1a2 and later).])

      # Add pthread library to $LIBS if we need it for threaded Tcl
      if test "x$ac_cv_lib_pthread" != x; then
        EGG_APPEND_VAR(LIBS, $ac_cv_lib_pthread)
      fi
    fi
  fi
])


dnl EGG_TCL_CHECK_GETTHREADDATA
dnl
AC_DEFUN([EGG_TCL_CHECK_GETTHREADDATA],
[
  if test "$enable_script_tcl" = "yes"; then
    if test "$egg_tcl_changed" = yes; then
      EGG_CACHE_UNSET(egg_cv_var_tcl_getthreaddata)
    fi

    # Check for Tcl_GetThreadData()
    AC_CHECK_LIB($TCL_TEST_LIB, Tcl_GetThreadData, [egg_cv_var_tcl_getthreaddata="yes"], [egg_cv_var_tcl_getthreaddata="no"], $TCL_TEST_OTHERLIBS)
    if test "$egg_cv_var_tcl_getthreaddata" = yes; then
      AC_DEFINE(HAVE_TCL_GETTHREADDATA, 1, [Define for Tcl that has Tcl_GetThreadData() (8.1a2 and later).])
    fi
  fi
])


dnl EGG_TCL_CHECK_SETNOTIFIER
dnl
AC_DEFUN([EGG_TCL_CHECK_SETNOTIFIER],
[
  if test "$enable_script_tcl" = "yes"; then
    if test "$egg_tcl_changed" = yes; then
      EGG_CACHE_UNSET(egg_cv_var_tcl_setnotifier)
    fi

    # Check for Tcl_SetNotifier()
    AC_CHECK_LIB($TCL_TEST_LIB, Tcl_SetNotifier, [egg_cv_var_tcl_setnotifier="yes"], [egg_cv_var_tcl_setnotifier="no"], $TCL_TEST_OTHERLIBS)
    if test "$egg_cv_var_tcl_setnotifier" = yes; then
      AC_DEFINE(HAVE_TCL_SETNOTIFIER, 1, [Define for Tcl that has Tcl_SetNotifier() (8.2b1 and later).])
    fi
  fi
])


dnl EGG_TCL_LIB_REQS()
dnl
AC_DEFUN([EGG_TCL_LIB_REQS],
[
  if test "$enable_script_tcl" = "yes"; then
    if test "$EGG_CYGWIN" = yes; then
      TCL_REQS="${TCLLIB}/lib${TCLLIBFN}"
      TCL_LIBS="-L$TCLLIB -l$TCLLIBFNS $EGG_MATH_LIB"
    else
      if test "$TCLLIBEXT" != ".a"; then
        TCL_REQS="${TCLLIB}/lib${TCLLIBFN}"
        TCL_LIBS="-L$TCLLIB -l$TCLLIBFNS $EGG_MATH_LIB"
      else
        # Set default make as static for unshared Tcl library
        if test "$DEFAULT_MAKE" != static; then
          cat << 'EOF' >&2
configure: WARNING:

  Your Tcl library is not a shared lib.
  configure will now set default make type to static.

EOF
          DEFAULT_MAKE="static"
          AC_SUBST(DEFAULT_MAKE)
        fi

        # Are we using a pre 7.4 Tcl version ?
        TCL_VER_PRE74=`echo $egg_cv_var_tcl_version | $AWK '{split([$]1, i, "."); if (((i[[1]] == 7) && (i[[2]] < 4)) || (i[[1]] < 7)) print "yes"; else print "no"}'`
        if test "$TCL_VER_PRE74" = no; then

          # Was the --with-tcllib option given ?
          if test "x$tcllibname" != x; then
            TCL_REQS="${TCLLIB}/lib${TCLLIBFN}"
            TCL_LIBS="${TCLLIB}/lib${TCLLIBFN} $EGG_MATH_LIB"
          else
            TCL_REQS="${TCLLIB}/lib${TCLLIBFN}"
            TCL_LIBS="-L$TCLLIB -l$TCLLIBFNS $EGG_MATH_LIB"
          fi
        else
          cat << EOF >&2
configure: WARNING:

  Your Tcl version ($egg_cv_var_tcl_version) is older than 7.4.
  There are known problems, but we will attempt to work around them.

EOF
          TCL_REQS="libtcle.a"
          TCL_LIBS="-L`pwd` -ltcle $EGG_MATH_LIB"
        fi
      fi
    fi
  fi

  AC_SUBST(TCL_REQS)
  AC_SUBST(TCL_LIBS)
])


