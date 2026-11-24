AC_PREREQ([2.69])
AC_INIT([crunch], [0.9.0], [bugs@example.invalid])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects no-dist-gzip dist-xz])
AC_CANONICAL_HOST
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_ARG_WITH([crunch-includedir],
  [AS_HELP_STRING([--with-crunch-includedir=DIR],
    [default directory searched by crunch-pp for headers])],
  [crunch_includedir=$withval; crunch_includedir_given=yes],
  [crunch_srcdir_abs=`cd "$srcdir" && pwd` || AC_MSG_ERROR([cannot resolve source directory])
   crunch_includedir="$crunch_srcdir_abs/include"])
AS_CASE([$crunch_includedir], [no|yes|""],
  [AC_MSG_ERROR([--with-crunch-includedir requires a directory])])
AS_CASE([$crunch_includedir], [/*], [], [crunch_includedir="`pwd`/$crunch_includedir"])
AC_SUBST([CRUNCH_INCLUDE_DIR], [$crunch_includedir])
dnl "make install" puts the headers in the explicitly configured directory, or
dnl else in $(includedir)/crunch; the installed crunch-pp is built to match.
AS_IF([test "x$crunch_includedir_given" = xyes],
  [crunch_install_includedir=$crunch_includedir],
  [crunch_install_includedir='${includedir}/crunch'])
AC_SUBST([CRUNCH_INSTALL_INCLUDEDIR], [$crunch_install_includedir])

AC_ARG_WITH([crunch-conffile],
  [AS_HELP_STRING([--with-crunch-conffile=FILE],
    [configuration file read by crunch-pp [/etc/crunch/crunch.conf]])],
  [crunch_conffile=$withval; crunch_conffile_given=yes],
  [crunch_conffile=/etc/crunch/crunch.conf])
AS_CASE([$crunch_conffile], [/*], [],
  [AC_MSG_ERROR([--with-crunch-conffile requires an absolute file name])])
AC_SUBST([CRUNCH_CONF_FILE], [$crunch_conffile])
AS_IF([test "x$crunch_conffile_given" = xyes],
  [crunch_install_conffile=$crunch_conffile],
  [crunch_install_conffile='${sysconfdir}/crunch/crunch.conf'])
AC_SUBST([CRUNCH_INSTALL_CONFFILE], [$crunch_install_conffile])

AS_CASE([$host_os], [linux*], [], [AC_MSG_ERROR([unsupported operating system '$host_os'; Crunch currently targets Linux])])
AS_CASE([$host_cpu], [x86_64|amd64], [], [AC_MSG_ERROR([unsupported machine '$host_cpu'; Crunch currently targets x86-64])])

AC_ARG_WITH([dynamic-linker],
  [AS_HELP_STRING([--with-dynamic-linker=FILE],
    [Override system detected dynamic linker])],
  [crunch_dynamic_linker=$withval],
  [AS_IF([test "x$cross_compiling" = xyes],
     [AC_MSG_ERROR([cross compilation requires --with-dynamic-linker=FILE])])
   crunch_dynamic_linker=
   for crunch_candidate in /lib64/ld-linux-x86-64.so.2 /lib/ld-linux-x86-64.so.2 /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2; do
     if test -f "$crunch_candidate" && test -x "$crunch_candidate"; then
       crunch_dynamic_linker=$crunch_candidate
       break
     fi
   done])
AS_IF([test -z "$crunch_dynamic_linker"],
  [AC_MSG_ERROR([cannot find an ELF interpreter; use --with-dynamic-linker=FILE])])
AS_IF([! printf '%s\n' "$crunch_dynamic_linker" | LC_ALL=C grep -Eq '^/[[a-zA-Z0-9_./+-]]+$'],
  [AC_MSG_ERROR([--with-dynamic-linker requires an absolute path using letters, digits, _, ., /, + or -])])
AC_MSG_NOTICE([crunch-ld default ELF interpreter: $crunch_dynamic_linker])
AC_SUBST([CRUNCH_DYNAMIC_LINKER], [$crunch_dynamic_linker])

AC_MSG_CHECKING([whether the C compiler supports strict C11])
crunch_save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS -std=c11 -Wall -Wextra -Wpedantic -Werror"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#error C11 support is required
#endif]], [[return 0;]])],
  [crunch_c11=yes], [crunch_c11=no])
CFLAGS=$crunch_save_CFLAGS
AC_MSG_RESULT([$crunch_c11])
AS_IF([test "x$crunch_c11" != xyes], [AC_MSG_ERROR([C compiler must support strict C11 with -Wall -Wextra -Wpedantic -Werror])])

AC_PATH_PROG([PYTHON], [python3])
AS_IF([test -z "$PYTHON"], [AC_MSG_ERROR([Python 3 is required for integration tests])])
AC_MSG_CHECKING([whether $PYTHON is Python 3])
crunch_python_major=`$PYTHON -c 'import sys; print(sys.version_info.major)' 2>/dev/null || echo no`
AS_IF([test "x$crunch_python_major" = x3], [crunch_python3=yes], [crunch_python3=no])
AC_MSG_RESULT([$crunch_python3])
AS_IF([test "x$crunch_python3" != xyes], [AC_MSG_ERROR([Python 3 is required for integration tests])])

AC_PATH_PROG([GNU_MAKE], [make])
AS_IF([test -z "$GNU_MAKE"], [AC_MSG_ERROR([GNU make is required])])
AC_MSG_CHECKING([whether make is GNU Make])
AS_IF(["$GNU_MAKE" --version 2>&1 | grep -q 'GNU Make'], [crunch_gnu_make=yes], [crunch_gnu_make=no])
AC_MSG_RESULT([$crunch_gnu_make])
AS_IF([test "x$crunch_gnu_make" != xyes], [AC_MSG_ERROR([GNU make is required])])

AC_ARG_ENABLE([native-only],
  [AS_HELP_STRING([--enable-native-only],
    [use Crunch assembler, linker and archiver without requiring GNU binutils])],
  [crunch_native_only=$enableval], [crunch_native_only=no])
AS_CASE([$crunch_native_only], [yes|no], [],
  [AC_MSG_ERROR([--enable-native-only accepts yes or no])])
AC_SUBST([CRUNCH_NATIVE_ONLY], [$crunch_native_only])
AC_ARG_WITH([default-assembler],
  [AS_HELP_STRING([--with-default-assembler=gnu|crunch],
    [default assembler used by crunch [auto-detect]])],
  [AS_CASE([$withval], [gnu|crunch], [],
     [AC_MSG_ERROR([--with-default-assembler must be gnu or crunch])])
   crunch_default_assembler=$withval], [crunch_default_assembler=auto])

AC_ARG_WITH([gnu-as],
  [AS_HELP_STRING([--with-gnu-as=FILE],
    [GNU assembler used by the crunch driver [search PATH for as]])],
  [crunch_as=$withval], [crunch_as=])
AC_ARG_WITH([crunch-as],
  [],
  [AS_IF([test -n "$crunch_as"],
     [AC_MSG_ERROR([--with-crunch-as and --with-gnu-as cannot be combined])])
   AC_MSG_WARN([--with-crunch-as=FILE is deprecated; use --with-gnu-as=FILE])
   crunch_as=$withval])
AC_ARG_WITH([crunch-ld],
  [AS_HELP_STRING([--with-crunch-ld=FILE],
    [GNU linker used by the crunch driver [search PATH for ld]])],
  [crunch_ld=$withval], [crunch_ld=])
AC_ARG_WITH([default-linker],
  [AS_HELP_STRING([--with-default-linker=gnu|crunch],
    [default linker used by crunch [auto-detect]])],
  [AS_CASE([$withval], [gnu|crunch], [],
     [AC_MSG_ERROR([--with-default-linker must be gnu or crunch])])
   crunch_default_linker=$withval], [crunch_default_linker=auto])
AC_ARG_WITH([runtime-assembler],
  [AS_HELP_STRING([--with-runtime-assembler=gnu|crunch],
    [assembler used to build the installed Crunch runtime [crunch]])],
  [crunch_runtime_assembler=$withval], [crunch_runtime_assembler=crunch])
AS_CASE([$crunch_runtime_assembler], [gnu|crunch], [],
  [AC_MSG_ERROR([--with-runtime-assembler must be gnu or crunch])])
AC_SUBST([CRUNCH_RUNTIME_ASSEMBLER], [$crunch_runtime_assembler])
AC_ARG_WITH([crunch-ar],
  [AS_HELP_STRING([--with-crunch-ar=FILE],
    [GNU archiver used when selected [search PATH for ar]])],
  [crunch_ar=$withval], [crunch_ar=])
AC_ARG_WITH([default-archiver],
  [AS_HELP_STRING([--with-default-archiver=gnu|crunch],
    [archiver used by crunch and to build Crunch libraries [auto-detect]])],
  [AS_CASE([$withval], [gnu|crunch], [],
     [AC_MSG_ERROR([--with-default-archiver must be gnu or crunch])])
   crunch_default_archiver=$withval], [crunch_default_archiver=auto])

AS_IF([test "x$crunch_native_only" = xyes], [
  AS_IF([test -n "$crunch_as" || test -n "$crunch_ld" || test -n "$crunch_ar" ||
         test "x$crunch_default_assembler" = xgnu || test "x$crunch_default_linker" = xgnu ||
         test "x$crunch_default_archiver" = xgnu || test "x$crunch_runtime_assembler" != xcrunch],
    [AC_MSG_ERROR([--enable-native-only cannot be combined with GNU tool selections])])
  AS=; LD=; CRUNCH_AR=
  crunch_gnu_as=not-probed; crunch_gnu_ld=not-probed; crunch_gnu_ar=not-probed
  crunch_detected_as='not probed'; crunch_detected_ld='not probed'; crunch_detected_ar='not probed'
], [
m4_foreach([crunch_spec], [[as,gnu-as], [ld,crunch-ld], [ar,crunch-ar]], [
  m4_pushdef([crunch_tool], m4_car(crunch_spec))
  m4_pushdef([crunch_option], m4_cdr(crunch_spec))
  m4_pushdef([crunch_var], m4_case(crunch_tool, [as], [AS], [ld], [LD], [ar], [CRUNCH_AR]))
  m4_pushdef([crunch_banner], m4_case(crunch_tool, [as], [GNU assembler], [ld], [GNU ld], [ar], [GNU ar]))
  AS_CASE([$crunch_[]crunch_tool], [yes|no],
    [AC_MSG_ERROR([--with-[]crunch_option requires a file name])])
  AS_IF([test -n "$crunch_[]crunch_tool"], [
    AS_CASE([$crunch_[]crunch_tool], [/*], [],
      [AC_MSG_ERROR([--with-[]crunch_option requires an absolute file name])])
    AS_IF([test -f "$crunch_[]crunch_tool" && test -x "$crunch_[]crunch_tool"], [],
      [AC_MSG_ERROR([--with-[]crunch_option: '$crunch_[]crunch_tool' is not an executable file])])
    crunch_var=$crunch_[]crunch_tool
    AC_SUBST(crunch_var)],
    [AC_PATH_PROG(crunch_var, [crunch_tool])])
  AC_MSG_CHECKING([whether crunch_tool is crunch_banner])
  AS_IF([test -n "$crunch_var" && "$crunch_var" --version 2>&1 | grep -q 'crunch_banner'],
    [crunch_gnu_[]crunch_tool=yes], [crunch_gnu_[]crunch_tool=no])
  AC_MSG_RESULT([$crunch_gnu_[]crunch_tool])
  AS_IF([test "x$crunch_gnu_[]crunch_tool" = xyes],
    [crunch_detected_[]crunch_tool="$crunch_var (GNU)"],
    [AS_IF([test -n "$crunch_var"], [crunch_detected_[]crunch_tool="$crunch_var (not GNU)"],
      [crunch_detected_[]crunch_tool='not found'])])
  AS_IF([test "x$crunch_gnu_[]crunch_tool" != xyes], [
    AS_IF([test -n "$crunch_[]crunch_tool"], [AC_MSG_ERROR([crunch_banner is required])])
    crunch_var=
  ])
  m4_popdef([crunch_banner])
  m4_popdef([crunch_var])
  m4_popdef([crunch_option])
  m4_popdef([crunch_tool])
])
])

AS_IF([test "x$crunch_default_assembler" = xauto],
  [AS_IF([test "x$crunch_gnu_as" = xyes], [crunch_default_assembler=gnu], [crunch_default_assembler=crunch])])
AS_IF([test "x$crunch_default_linker" = xauto],
  [AS_IF([test "x$crunch_gnu_ld" = xyes], [crunch_default_linker=gnu], [crunch_default_linker=crunch])])
AS_IF([test "x$crunch_default_archiver" = xauto],
  [AS_IF([test "x$crunch_gnu_ar" = xyes], [crunch_default_archiver=gnu], [crunch_default_archiver=crunch])])
AS_IF([test "x$crunch_default_assembler" = xgnu || test "x$crunch_runtime_assembler" = xgnu],
  [AS_IF([test "x$crunch_gnu_as" != xyes], [AC_MSG_ERROR([GNU assembler is required])])])
AS_IF([test "x$crunch_default_linker" = xgnu],
  [AS_IF([test "x$crunch_gnu_ld" != xyes], [AC_MSG_ERROR([GNU ld is required])])])
AS_IF([test "x$crunch_default_archiver" = xgnu],
  [AS_IF([test "x$crunch_gnu_ar" != xyes], [AC_MSG_ERROR([GNU ar is required])])])
AC_SUBST([CRUNCH_DEFAULT_ASSEMBLER], [$crunch_default_assembler])
AC_SUBST([CRUNCH_DEFAULT_LINKER], [$crunch_default_linker])
AC_SUBST([CRUNCH_DEFAULT_ARCHIVER], [$crunch_default_archiver])
AC_MSG_NOTICE([Tool selection (configuration file may override driver defaults):])
AC_MSG_NOTICE([  assembler: detected $crunch_detected_as; driver $crunch_default_assembler; runtime $crunch_runtime_assembler])
AC_MSG_NOTICE([  linker:    detected $crunch_detected_ld; driver $crunch_default_linker (dynamic Crunch links use crunch-ld)])
AC_MSG_NOTICE([  archiver:  detected $crunch_detected_ar; driver and runtime $crunch_default_archiver])

AC_ARG_WITH([crunch-libdir],
  [AS_HELP_STRING([--with-crunch-libdir=DIR],
    [directory containing libcrunch.a for the installed crunch driver [LIBDIR/crunch]])],
  [crunch_libdir=$withval; crunch_libdir_given=yes], [crunch_libdir=])
AS_IF([test "x$crunch_libdir_given" = xyes],
  [AS_CASE([$crunch_libdir], [/*], [],
     [AC_MSG_ERROR([--with-crunch-libdir requires an absolute directory name])])
   crunch_install_libdir=$crunch_libdir],
  [crunch_install_libdir='${libdir}/crunch'])
AC_SUBST([CRUNCH_INSTALL_LIBDIR], [$crunch_install_libdir])

AC_CONFIG_FILES([Makefile build/config.mk build/crunch-linker-config.h])
AC_OUTPUT
