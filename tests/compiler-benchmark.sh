#!/bin/sh

columns="rand32_sRGB_mixed_l faster_wavg_a faster_wavg_l"

t()
{
	echo >&2 "Testing $*..."
	echo -n "$*"

	good=false
	eval `use_external=false sh test.sh --disable-shared --enable-static "$@"`
	if $good; then
		for c in $columns; do
			eval "v=\$$c"
			echo -n "	$v"
		done
		echo
	else
		for c in $columns; do
			echo -n "	FAILED"
		done
		echo
	fi
}

#t CXX=/opt/ekopath/bin/pathcc CXXFLAGS="-O3" LDFLAGS="/usr/lib/gcc/x86_64-linux-gnu/4.2.4/libstdc++.a"
t CXX=/opt/intel/bin/icc CXXFLAGS="-O3"
t CXX=/opt/intel/bin/icc AR=/opt/intel/bin/xiar CXXFLAGS="-xHOST -O3 -ipo -no-prec-div"
t CXX=g++ CXXFLAGS="-O3"
t CXX=g++ CXXFLAGS="-Ofast"
t CXX=clang++ CXXFLAGS="-O3"
t CXX=clang++ CXXFLAGS="-O4"
