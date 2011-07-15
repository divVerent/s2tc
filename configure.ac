AC_INIT([s2tc], [0.1], [divVerent@xonotic.org])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall foreign])

have_CXXFLAGS=${CXXFLAGS+set}
AC_PROG_CXX
# better default optimization flags for g++, please
if test "$have_CXXFLAGS" != set; then
	if test "$GXX" = yes; then
		CXXFLAGS="-g -O3"
	fi
fi

AC_PROG_LIBTOOL

AC_CONFIG_FILES([Makefile txc_dxtn.pc])

AC_OUTPUT
