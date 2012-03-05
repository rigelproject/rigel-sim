AC_INIT([rigelsim],[1.0])
AC_CONFIG_SRCDIR([src/sim.cpp])
AM_INIT_AUTOMAKE([subdir-objects])

AC_CANONICAL_BUILD

#Autoconf will make CXXFLAGS/CFLAGS "-g -O2" if no user-specified CXXFLAGS/CFLAGS are available.
#This is a decent default for development, but we want at least an option with all optis and no debug symbols.
#The below lines explicitly set flags to blank if the user hasn't specified any.
#The badness happens in the AC_PROG_CXX and AC_PROG_CC macros, so the explicit blanking has to be done before them.
test ".$CFLAGS" = "." && CFLAGS=""
test ".$CXXFLAGS" = "." && CXXFLAGS=""

AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_CONFIG_FILES([Makefile])

#config.h
AC_CONFIG_HEADERS([include/config.h])

#Check library dependencies
#libm
AC_SEARCH_LIBS([cos],[m], [], [
  AC_MSG_ERROR([Unable to find libm])
])
#libelf
AC_SEARCH_LIBS([elf32_getehdr], [elf], [], [
  AC_MSG_ERROR([Unable to find a libelf with elf32_getehdr()])
])
AC_CHECK_HEADERS([libelf.h],
	[rigel_found_libelf_header=yes; break;])
AS_IF([test "x$rigel_found_libelf_header" != "xyes"],
	[AC_MSG_ERROR([Unable to find libelf headers])])

#zlib
AC_SEARCH_LIBS([zlibVersion], [z], [], [
  AC_MSG_ERROR([Unable to find libz (zlib)])
])

#pthread
#TODO: Is this check redundant with PKG_CHECK_MODULES(protobuf) below?
AC_SEARCH_LIBS([pthread_create], [pthread], [], [
  AC_MSG_ERROR([Unable to find libpthread])
])

#RandomLib
#Checking for a library with no free c-linkage functions is icky.
#Check cribbed from http://nerdland.net/2009/07/detecting-c-libraries-with-autotools/
AC_LANG_PUSH([C++])
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -L${RIGEL_INSTALL}/host/lib -lRandom"
SAVED_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS -I${RIGEL_INSTALL}/host/include"
AC_MSG_CHECKING(for RandomLib)
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([#include <RandomLib/Random.hpp>],
    [RandomLib::Random RNG])],
  [TEST_LIBS="$TEST_LIBS -L${RIGEL_INSTALL}/host/lib -lRandom"] [have_randomlib=yes],
  [have_randomlib=no])
CPPFLAGS=$SAVED_CPPFLAGS
LDFLAGS=$SAVED_LDFLAGS
AC_LANG_POP([C++])
if test "x$have_randomlib" = "xyes"; then
  AC_MSG_RESULT(yes)
	LIBS="$LIBS -lRandom"
else
  AC_MSG_RESULT(no)
	AC_MSG_ERROR([Unable to find RandomLib])
fi

#Flags
#CouchDB
AC_ARG_ENABLE([couchdb],
              AS_HELP_STRING([--enable-couchdb], [Enable CouchDB support]))

AM_CONDITIONAL(ENABLE_COUCHDB, test x$enable_couchdb = xyes)

#FIXME If we are going to continue to support this binding, we should
#include AC_SEARCH_LIBS calls for each of these CouchDB deps
if test "x$enable_couchdb" = "xyes"
then
	LIBS="$LIBS -luuid -L/usr/local/lib -lcurlpp -lstdc++ -lcurl -lgssapi_krb5"
	AC_DEFINE([ENABLE_COUCHDB], [1], [Define if CouchDB support is enabled.])
fi

#GPL components (e.g., disassembly via libopcodes)
AC_ARG_ENABLE([gplv3],
              AS_HELP_STRING([--enable-gplv3], [Enable linking with GPLv3-licensed libraries without linking exceptions (e.g., libopcodes).  WARNING: If enabled, the resulting simulator is also licensed under GPLv3. ]))

AM_CONDITIONAL(ENABLE_GPLV3, test x$enable_gplv3 = xyes)

if test "x$enable_gplv3" = "xyes"
then
	#NOTE: -lbfd must come before -lopcodes because shared libopcodes doesn't declare
	#its dependence on libbfd for bfd_getl32() and friends, so we have to ensure the
	#dependence is met by linking them in the correct order.
	#NOTE: This order is only valid if we're using the shared libraries, which we
	#should be by default.  If we want to statically link these, the order must be
	#the other way around ('-lliberty -lbfd -lopcodes')
	#See http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=338907 for slightly more info
	#and another approach to solving the problem.
	#TODO Should we append to LIBS directly, or put it in its own variable which can be included in relevant binaries?
  LIBS="$LIBS -lbfd -lopcodes -liberty"
  AC_DEFINE([ENABLE_GPLV3], [1], [Define if linking with GPLv3 libraries is allowed.])
fi

AC_ARG_ENABLE(debug,
AS_HELP_STRING([--enable-debug],
               [enable debugging, default: no]),
[case "${enableval}" in
             yes) debug=true ;;
             no)  debug=false ;;
             *)   AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
esac],
[debug=false])

AM_CONDITIONAL(DEBUG, test x"$debug" = x"true")

AC_ARG_ENABLE(asserts,
AS_HELP_STRING([--enable-asserts],
               [enable assertions, default: no]),
[case "${enableval}" in
             yes) asserts=true ;;
             no)  asserts=false ;;
             *)   AC_MSG_ERROR([bad value ${enableval} for --enable-asserts]) ;;
esac],
[asserts=false])

AM_CONDITIONAL(ASSERTS, test x"$asserts" = x"true")

PKG_CHECK_MODULES([protobuf], [protobuf])

AC_OUTPUT
