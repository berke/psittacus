AC_INIT([psittacus], [1.0], [berke.durak@gmail.com])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_PROG_CXX
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

PKG_CHECK_MODULES(GLOOX, gloox)
PKG_CHECK_MODULES(LIBIDN, libidn)
PKG_CHECK_MODULES(LIBGNUTLS, gnutls)

# *** FIXME ***
# For some reason libgcrypt is never found by PKG_CHECK_MODULES, although it should
#PKG_CHECK_MODULES(LIBGCRYPT, libgcrypt)

AC_OUTPUT
