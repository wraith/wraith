AC_DEFUN([CHECK_LIBELF],
[
  LIBELF_DIR="lib/libelf/lib"
  LIBELF_LIB="\$(top_builddir)/${LIBELF_DIR}/libelf.a"
  LIBELF_BUNDLED="${LIBELF_LIB}"
  LIBELF_INCLUDE="-I\$(top_srcdir)/${LIBELF_DIR}"

  AC_CHECK_HEADERS([gelf.h libelf.h], [
    AC_CHECK_TYPES([Elf_Note], [
      AC_DEFINE(HAVE_GELF_H, 1, [Define to 1 if you have the <gelf.h> header file.])
      AC_CHECK_LIB(elf, gelf_getehdr, [
        AC_DEFINE(HAVE_LIBELF, 1, [Define to 1 if you have the 'elf' library (-lelf).])
        LIBELF_LIB="-Wl,-Bstatic -lelf -Wl,-Bdynamic"
        LIBELF_BUNDLED=
        LIBELF_INCLUDE=
      ]) dnl AC_CHECK_LIB
    ], dnl AC_CHECK_TYPES
    [],
    AC_LANG_SOURCE(
      [[
#include "gelf.h"
#include "libelf.h"
      ]])
    )
  ])

  CPPFLAGS="${CPPFLAGS} ${LIBELF_INCLUDE}"

  AC_SUBST([LIBELF_BUNDLED])
  AC_SUBST([LIBELF_LIB])
])
