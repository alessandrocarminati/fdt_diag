AC_INIT([fdt], [0.1], [alessandro.carminati@gmail.com])
AM_INIT_AUTOMAKE([foreign])
AC_PROG_CC
AC_PROG_LIBTOOL
AC_CONFIG_HEADERS([config.h])

# Check for required libraries
AC_CHECK_LIB([fdt], [fdt_getprop], [], [AC_MSG_ERROR([libfdt is required])])
AC_CHECK_LIB([crypto], [MD5], [], [AC_MSG_ERROR([openssl is required])])

# Output files
AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
