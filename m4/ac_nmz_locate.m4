# Find Normaliz in a given path as an argument or in typical paths

AC_DEFUN([AC_NMZ_LOCATE],
[
  LOCATIONS="/opt /usr/local /usr"
  AC_ARG_WITH(normaliz,
    AS_HELP_STRING([--with-normaliz=<path>], 
      [specify Normaliz installation path, otherwise, the library is 
      searched in the standard locations (/opt, /usr or /usr/local)]),
    [if test "x$withval" != "x"; then
      LOCATIONS="$withval ${LOCATIONS}"
    fi],
    []
  )

  # Check for existence
  old_CPPFLAGS=${CPPFLAGS}
  old_LIBS=${LIBS}

  AC_MSG_CHECKING(for Normaliz)
  locate_normaliz=no
  for L in ${LOCATIONS}; do
    if test "x$locate_normaliz" != "xyes"; then
      # AC_MSG_CHECKING(in ${L})
      if test -r "$L/include/libnormaliz/version.h"; then
        locate_normaliz=yes
        NMZ_CPPFLAGS="-I${L}/include"
        NMZ_LIBS="-L${L}/lib -lnormaliz"
        CPPFLAGS="${CPPFLAGS} ${NMZ_CPPFLAGS}"
        LIBS="${LIBS} ${NMZ_LIBS}"
        AC_LINK_IFELSE([
          AC_LANG_PROGRAM(
            [#include <vector>
            #include <libnormaliz/libnormaliz.h>
            #include <libnormaliz/cone.h>],
            [std::vector<std::vector<long>> dummy_gen(1, std::vector<long>(1, 1));
            libnormaliz::Cone<long> a(libnormaliz::Type::cone, dummy_gen)])],
          [NORMALIZ=${L}
          AC_SUBST(NORMALIZ)],
          [locate_normaliz=no
          unset NMZ_CPPFLAGS
          unset NMZ_LIBS
          CPPFLAGS=${old_CPPFLAGS}
          LIBS=${old_LIBS}]
        )
      fi
    fi
  done
  AC_SUBST(locate_normaliz)

  CPPFLAGS=${old_CPPFLAGS}
  LIBS=${old_LIBS}

  if test "x$locate_normaliz" != "xno"; then
    AC_MSG_RESULT([found])
  else
    AC_MSG_RESULT([not found])
  fi
])
