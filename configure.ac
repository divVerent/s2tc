AC_INIT([s2tc],[0.1],[divVerent@xonotic.org])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall foreign])

have_CXXFLAGS=${CXXFLAGS+set}
AC_PROG_CXX
AM_PROG_AR
# better default optimization flags for g++, please
if test "$have_CXXFLAGS" != set; then
	if test "$GXX" = yes; then
		CXXFLAGS="-g -O3"
	fi
fi

LT_INIT

# Disable dependency on libstdc++ for the .so library.
postdeps_CXX=`echo " $postdeps_CXX " | sed 's, -lstdc++ , ,g'`

AC_ARG_ENABLE(runtime-linking, AS_HELP_STRING([--disable-runtime-linking], [Do not load the library at runtime (faster startup, more dependencies)]), [enable_runtime_linking=$enableval], [enable_runtime_linking=yes])
AC_ARG_ENABLE(tools, AS_HELP_STRING([--disable-tools], [Do not build the s2tc_compress and s2_decompress tools]), [enable_tools=$enableval], [enable_tools=yes])
AC_ARG_ENABLE(lib, AS_HELP_STRING([--disable-lib], [Do not build the included libtxc_dxtn library for S2TC]), [enable_lib=$enableval], [enable_lib=yes])

AM_CONDITIONAL(ENABLE_RUNTIME_LINKING, [test x"$enable_runtime_linking" != xno])
AM_CONDITIONAL(ENABLE_TOOLS, [test x"$enable_tools" != xno])
AM_CONDITIONAL(ENABLE_LIB, [test x"$enable_lib" != xno])

AC_CHECK_HEADERS([GL/gl.h], , [AC_MSG_ERROR([OpenGL includes not found])])

AS_IF([test x"$enable_runtime_linking" = xno], ,
	[AS_IF([test x"$enable_dlopen" != xno], ,
		[AC_MSG_ERROR([dynamic linking not possible, try --disable-runtime-linking])])])

if test x"$enable_runtime_linking" = xyes; then
	case "$host_os" in
	linux*)
		LIBDL_LDADD='-ldl'
		;;
	esac
fi
AC_SUBST(LIBDL_LDADD)

AC_CONFIG_FILES([Makefile txc_dxtn.pc])

AC_OUTPUT
