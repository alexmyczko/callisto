#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([callisto], [1.1.0], [jha@kurp.hut.fi])
AC_CONFIG_SRCDIR([src/callisto.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

callisto_have_pthread=n
ACX_PTHREAD([callisto_have_pthread=y])
AC_CHECK_HEADER([pthread.h], , [callisto_have_pthread=n])
if test "x$callisto_have_pthread" = "xn" ; then
   AC_MSG_ERROR([working POSIX threads not found])
fi
CC="$PTHREAD_CC"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
LIBS="$LIBS $PTHREAD_LIBS"

AC_CHECK_LIB([cfitsio], [ffopen],,
             AC_MSG_ERROR([CFITSIO library not found]))

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h stdlib.h string.h sys/file.h sys/ioctl.h sys/time.h syslog.h sys/socket.h termios.h unistd.h fitsio.h],,
		 AC_MSG_ERROR([required headers not found]))

# Checks for typedefs, structures, and compiler characteristics.
AC_SYS_LARGEFILE
AC_TYPE_INT64_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UID_T
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_FORK
AC_FUNC_MKTIME
AC_CHECK_FUNCS([dup2 ftruncate gettimeofday localtime_r memset setenv socket strcasecmp strchr strdup strerror strrchr strspn strstr strtol timegm tzset])

if test "x$GCC" = "xyes" ; then
   AC_SUBST(WARNINGFLAGS, ["-Wall"])
   CFLAGS="-std=gnu99 $CFLAGS"
fi

CPPFLAGS="$CPPFLAGS -DETCDIR='\"\$(sysconfdir)/callisto\"'"


AC_CONFIG_FILES([Makefile src/Makefile man/Makefile])
AC_OUTPUT
