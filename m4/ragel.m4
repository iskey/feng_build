dnl Check for presence of the Ragel State Machine generator.
dnl
dnl This macro checks for the presence of the ragel tool in the system,
dnl and whether the ragel tool is absolutely needed for a complete
dnl build.
dnl
dnl To check for the need for Ragel, you have to provide the relative
dnl path of a source file generated through Ragel: if the file is
dnl present in the source tree, a missing ragel command will not cause
dnl the configure to abort.

AC_DEFUN([_RAGEL_VARS], [
  AC_ARG_VAR([RAGEL], [Ragel generator command])
  AC_ARG_VAR([RAGELFLAGS], [Ragel generator flags])
])

AC_DEFUN([CHECK_RAGEL], [
  AC_REQUIRE([_RAGEL_VARS])
  AC_CHECK_PROG([RAGEL], [ragel], [ragel], [no])

  dnl We set RAGEL to false so that it would execute the "false"
  dnl command if needed.
  AS_IF([test x"$RAGEL" = x"no"], [RAGEL=false])

  dnl Only test the need if not found
  AS_IF([test x"$RAGEL" = x"false"], [
    AC_MSG_CHECKING([whether we need ragel to regenerate sources])
    AS_IF([test -a ${srcdir}/$1], [ragel_needed=no], [ragel_needed=yes])
    AC_MSG_RESULT([$ragel_needed])

    AS_IF([test x"$ragel_needed" = x"yes"],
      [AC_MSG_ERROR([dnl
You need Ragel to build from GIT checkouts.
You can find Ragel at http://www.complang.org/ragel/dnl
      ])])
  ])
])

AC_DEFUN([CHECK_RAGEL_AM], [
  CHECK_RAGEL([$1])

  AM_CONDITIONAL([HAVE_RAGEL], [test x"$RAGEL" != x"false"])
])
