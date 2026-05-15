AC_PREREQ([2.69])
AC_INIT([lumila-ai], [0.6.0], [bugs@example.com])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Werror])

# Checks for programs
AC_PROG_CC
AM_PROG_AR
LT_INIT([disable-static])

# pkg-config
PKG_PROG_PKG_CONFIG

# Required packages
PKG_CHECK_MODULES([GEANY], [geany >= 1.38])
PKG_CHECK_MODULES([GTK], [gtk+-3.0 >= 3.20])
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.50])
PKG_CHECK_MODULES([SOUP], [libsoup-3.0], 
    [AC_DEFINE([HAVE_SOUP3], [1], [libsoup 3.x available])],
    [PKG_CHECK_MODULES([SOUP], [libsoup-2.4 >= 2.60],
        [AC_DEFINE([HAVE_SOUP2], [1], [libsoup 2.x available])])]
)
PKG_CHECK_MODULES([JANSSON], [jansson >= 2.7])

# Export version to config.h
AC_DEFINE_UNQUOTED([LUMILA_VERSION], ["$VERSION"], [Plugin version])
AC_DEFINE_UNQUOTED([LUMILA_NAME], ["Lumila AI"], [Plugin display name])
AC_DEFINE_UNQUOTED([LUMILA_DESCRIPTION], ["AI Assistant for Geany - Multi-provider Chat"], [Plugin description])
AC_DEFINE_UNQUOTED([LUMILA_AUTHOR], ["Iván Gabriel Sosa"], [Plugin author])

# Plugin directory
AC_MSG_CHECKING([for Geany plugin directory])
PLUGIN_DIR=$($PKG_CONFIG --variable=libdir geany)/geany
AC_MSG_RESULT([$PLUGIN_DIR])
AC_SUBST([PLUGIN_DIR])

AC_CONFIG_FILES([
    Makefile
    src/Makefile
    data/Makefile
])

AC_OUTPUT

echo "
Configuration:
  Version:     ${VERSION}
  Prefix:      ${prefix}
  Plugin dir:  ${PLUGIN_DIR}
"
