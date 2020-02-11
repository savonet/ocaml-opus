AC_INIT([ocaml-opus],[0.1.4],[savonet-users@lists.sourceforge.net])

VERSION=$PACKAGE_VERSION
AC_MSG_RESULT(configuring $PACKAGE_STRING)

AC_BASE_CHECKS()

PKG_PROG_PKG_CONFIG()
PKG_CONFIG_CHECK_MODULE([opus], [1.3.0])

# Include a config.h
AC_CONFIG_HEADERS([config.h:config.h.in])
# Include it
CFLAGS="$CFLAGS -I.."

# Check endianess
AC_C_BIGENDIAN(AC_DEFINE([BIGENDIAN], [1], [The target is big endian]),[])

AC_ARG_WITH([ogg-dir],AS_HELP_STRING([--with-ogg-dir=path],[use "path" as the location of ocaml-ogg (autodetected by default)]))
if test -z "$with_ogg_dir"; then
  AC_MSG_CHECKING(for ocaml-ogg)
  if ($OCAMLFIND query ogg > /dev/null 2>&1); then
	OCAMLOGG_INC=`$OCAMLFIND query ogg`
	AC_MSG_RESULT(ok)
  else
	AC_MSG_ERROR(not found.)
  fi
else
  echo $with_ogg_dir | grep ^/ > /dev/null 2>&1 \
  || with_ogg_dir=$PWD/$with_ogg_dir
  OCAMLOGG_INC="$with_ogg_dir"
fi
AC_SUBST(OCAMLOGG_INC)

# substitutions to perform
AC_SUBST(VERSION)

# Finally create the Makefile and samples
AC_CONFIG_FILES([Makefile],[chmod a-w Makefile])
AC_CONFIG_FILES([src/Makefile],[chmod a-w src/Makefile])
AC_CONFIG_FILES([src/META])
AC_CONFIG_FILES([examples/Makefile.opus2wav],[chmod a-w examples/Makefile.opus2wav])
AC_OUTPUT
