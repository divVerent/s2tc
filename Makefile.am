ACLOCAL_AMFLAGS = -I m4
bin_PROGRAMS = s2tc_compress s2tc_decompress
s2tc_compress_SOURCES = s2tc_compress.cpp txc_dxtn.h s2tc_license.h
s2tc_compress_LDADD = libtxc_dxtn.la
s2tc_decompress_SOURCES = s2tc_decompress.cpp txc_dxtn.h s2tc_license.h
s2tc_decompress_LDADD = libtxc_dxtn.la

lib_LTLIBRARIES = libtxc_dxtn.la

libtxc_dxtn_la_SOURCES = s2tc_algorithm.cpp s2tc_libtxc_dxtn.cpp s2tc_common.h s2tc_algorithm.h txc_dxtn.h s2tc_license.h
libtxc_dxtn_la_LDFLAGS = -versioninfo 0:0:0
libtxc_dxtn_la_LIBADD = -lm
libtxc_dxtn_la_CFLAGS = -fvisibility=hidden -Wold-style-definition -Wstrict-prototypes -Wsign-compare -Wdeclaration-after-statement

library_includedir = $(includedir)
library_include_HEADERS = txc_dxtn.h
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_HEADERS = txc_dxtn.pc

# versioninfo:
#   - compatible interface change: c:r:a -> c+1:0:a+1
#   - incompatible interface change: c:r:a -> c+1:0:0
#   - internal change: c:r:a -> c:r+1:a

EXTRA_DIST = README.txt autogen.sh
