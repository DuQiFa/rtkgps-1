dnl autoconf source for rtkgps installation
dnl Brendt Wohlberg
dnl Most recent modification: 4 February 2012

dnl Process this file with autoconf to produce a configure script
m4_define(VERSION, 0.07)
AC_INIT(RtkGPS, VERSION, osspkg@gmail.com, rtkgps)

AC_LANG_C

dnl Check for C compiler
AC_PROG_CC
if test "$GCC" = "yes"; then
  CFLAGS="$CFLAGS -ffloat-store -Wall -pedantic -W"
  dnl Check whether compiler accepts -Wno-missing-field-initializers flag
  AC_MSG_CHECKING([if gcc accepts -Wno-missing-field-initializers option])
  echo "int main() { return 0; }" > tmptest.c
  if $CC -Wno-missing-field-initializers -c tmptest.c 2>/dev/null; then
    AC_MSG_RESULT([yes])
    CFLAGS="$CFLAGS -Wno-missing-field-initializers"
  else
    AC_MSG_RESULT([no])
  fi
  rm -f tmptest.c tmptest.o
fi

AC_C_BIGENDIAN

AC_PROG_INSTALL

dnl Find path to sh, rm, and bzcat
AC_PATH_PROG(SH, sh)
AC_PATH_PROG(RM, rm)
AC_PATH_PROG(BZCAT, bzcat)

dnl Find path to (and version number of) python
AC_PATH_PROG(PYTHON, python)
if test "$PYTHON" != ''; then
  PYTHONVER=`"$PYTHON" -V 2>&1 | cut -d ' ' -f 2 | cut -d '.' -f -2`
  if expr "$PYTHONVER" \< 2 \| "$PYTHONVER" \>= 3 > /dev/null; then
    AC_PATH_PROGS(PYTHON, python2 python2.6 python2.5 python2.4 python2.3 python2.2)
  fi
fi

dnl Check for standard header files
AC_HEADER_STDC
AC_CHECK_HEADERS(assert.h ctype.h errno.h fcntl.h getopt.h math.h stdio.h stdint.h stdlib.h string.h sys/ioctl.h sys/mman.h sys/stat.h sys/types.h termios.h time.h unistd.h,,
AC_MSG_ERROR(Could not locate all of the required system headers)
	)

dnl Check whether cfmakeraw is available
AC_MSG_CHECKING(whether cfmakeraw is available)
AC_TRY_LINK([
#include <termios.h>
],
[
struct termios t;

cfmakeraw(&t);
],
AC_MSG_RESULT(yes); AC_DEFINE(HAVE_CFMAKERAW, 1),
[AC_MSG_RESULT(no)]
)

dnl Check whether TIOCGWINSZ is available
AC_MSG_CHECKING(whether TIOCGWINSZ is available)
AC_TRY_LINK([
#include <sys/ioctl.h>
],
[
struct winsize ws;

ioctl(1, TIOCGWINSZ, (void *)&ws);
],
AC_MSG_RESULT(yes); AC_DEFINE(HAVE_TIOCGWINSZ, 1),
[AC_MSG_RESULT(no)]
)

dnl Define --enable-bluetooth option
btenable=yes
AC_ARG_ENABLE(bluetooth,
 AS_HELP_STRING(--enable-bluetooth, enable bluetooth support (default is yes)),
	[if test "$enableval" = "no"; then btenable=no; fi])


dnl Check for bluez headers
if test "$btenable" = "yes"; then
AC_MSG_CHECKING(whether bluetooth headers are available)
AC_TRY_COMPILE([
#include <bluetooth/bluetooth.h> 
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>
],
[
bdaddr_t baddr;
inquiry_info *inqp = 0;
],
AC_MSG_RESULT(yes),
[AC_MSG_RESULT(no); AC_MSG_WARN(bluetooth headers not found); btenable="no"]
)
fi

dnl Check for bluez library
if test "$btenable" = "yes"; then
LDFLAGSTMP=$LDFLAGS
LDFLAGS="$LDFLAGS -lbluetooth"
AC_MSG_CHECKING(whether bluetooth library is available)
AC_TRY_LINK([
#include <bluetooth/bluetooth.h> 
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>
],
[
int adid;
bdaddr_t baddr;
char *baddrs = "00:00:00:00:00:00";

adid = hci_get_route(NULL);
ba2str(&baddr, baddrs);
],
AC_MSG_RESULT(yes),
[AC_MSG_RESULT(no); AC_MSG_WARN(bluetooth library not found); btenable="no"]
)
LDFLAGS=$LDFLAGSTMP
fi

dnl Set up bluetooth support if enabled and headers and library found
if test "$btenable" = "yes"; then
  LDFLAGS="$LDFLAGS -lbluetooth"
  AC_DEFINE(ENABLE_LINUX_BT, 1)
fi

dnl Add maths library for linker
LDFLAGS="$LDFLAGS -lm"

dnl Define --enable-geoidcor option
geoidcor='yes'
AC_ARG_ENABLE(geoidcor, 
   AS_HELP_STRING(--enable-geoidcor, enable geoid correction (default is yes)),
   geoidcor=$enableval,geoidcor=yes)
GEOIDCOR=$geoidcor
if test "$geoidcor" = "yes"; then 
  AC_DEFINE(GEOIDCOR,1)
fi
AC_SUBST(GEOIDCOR)

dnl Define --enable-debug option
AC_ARG_ENABLE(debug, 
	AS_HELP_STRING(--enable-debug, enable debugging (default is no)),
	[if test "$enableval" = "yes" -o "$enableval" = ""; then 
	   AC_DEFINE(DEBUG,1)
         fi])

AC_OUTPUT(Makefile)
