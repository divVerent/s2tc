#!/bin/sh

columns="rand32_sRGB_mixed_l rand32_wavg_l rand32_avg_l norand_wavg_a faster_wavg_a faster_wavg_l"

t()
{
	echo >&2 "Testing $*..."
	echo -n "$*"

	good=false
	eval `use_external=false sh test.sh "$@"`
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

t CXX=g++ CXXFLAGS="-O3"
t CXX="g++" CXXFLAGS="-Ofast"
t CXX=clang++ CXXFLAGS="-O3"
t CXX="clang++" CXXFLAGS="-O4"
