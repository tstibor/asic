#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])

# Get version information from git.
m4_define([GIT_VERSION_STRING], m4_esyscmd([git describe --tags | sed 's/\(.*\)-.*/\1/' | tr -d '\n']))

AC_INIT([asic], [GIT_VERSION_STRING], [thomas@stibor.net], [], [https://github.com/tstibor/asic])
AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_CONFIG_SRCDIR([src/asic.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_RANLIB

# Use the C language and compiler for the following checks.
AC_LANG([C])

# Path to ASI SDK directory.
AC_ARG_WITH([asi-sdk], AS_HELP_STRING([--with-asi-sdk[=PATH]],[path to asi-sdk [default=/opt/asi/0.7.0118]]),
		       ASI_SDK_DIR="$withval", ASI_SDK_DIR="/opt/asi/0.7.0118")
AC_CHECK_FILE("$ASI_SDK_DIR/include/ASICamera2.h",,
AC_MSG_ERROR("cannot find asi-sdk files. Use --with-asi-sdk=PATH"))

LDFLAGS="$LDFLAGS -L $ASI_SDK_DIR/lib/x64"

# Checks for other libraries.
AC_CHECK_LIB([ASICamera2], [ASIGetNumOfConnectedCameras, ASIGetCameraProperty, ASIOpenCamera, ASIInitCamera],
	     [], [AC_MSG_ERROR([cannot find asi-sdk library ASICamera, provide library path e.g. ./configure LDFLAGS='-L/<PATH_TO_LIB>'])])

AC_CHECK_LIB([tiff], [TIFFOpen, TIFFSetField, TIFFWriteEncodedStrip, TIFFClose],
	     [], [AC_MSG_ERROR([cannot find tiff library, provide library path e.g. ./configure LDFLAGS='-L/<PATH_TO_LIB>'])])

AC_CHECK_LIB([cfitsio], [ffiopn],
	     [], [AC_MSG_ERROR([cannot find cfitsio library, provide library path e.g. ./configure LDFLAGS='-L/<PATH_TO_LIB>'])])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h tiffio.h])

# Propage flags and dirs among final Makefiles.
AC_SUBST([AM_CFLAGS])
AC_SUBST([ASI_SDK_DIR])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL

# Checks for library functions.

AC_CONFIG_FILES([Makefile
		 src/Makefile
		 src/lib/Makefile])

# Remove unneeded libraries.
LDFLAGS="$LDFLAGS -Wl,--as-needed"

# Halt on warnings.
CFLAGS="$CFLAGS -Wall -Werror"

AC_OUTPUT
