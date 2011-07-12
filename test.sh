#!/bin/sh

set -ex

CXX="g++ -Wall -Wextra -O3"
$CXX s2tc.cpp -o s2tc

mkdir -p html
exec 3>html/index.html

html_start()
{
	echo >&3 "<html><title>S2TC</title><body><h1>S2TC</h1>"
	echo >&3 "<table>"
	echo >&3 "<tr><th>Picture</th>"
	echo >&3 "<th>Original</th>"
	echo >&3 "<th>nvcompress</th>"
	echo >&3 "<th>rand64-sRGB</th>"
#	echo >&3 "<th>norand-sRGB</th>"
	echo >&3 "<th>rand64-sRGB-mixed</th>"
#	echo >&3 "<th>norand-sRGB-mixed</th>"
	echo >&3 "<th>rand64-YUV</th>"
#	echo >&3 "<th>norand-YUV</th>"
	echo >&3 "<th>rand64</th>"
#	echo >&3 "<th>norand</th>"
	echo >&3 "<th>rand64-avg</th>"
#	echo >&3 "<th>norand-avg</th>"
	echo >&3 "</tr>"
}
html_rowstart()
{
	echo >&3 "<tr><th>$1</th>"
}
html()
{
	convert "$1" -crop 256x256+128+128 "html/$1.png"
	echo >&3 "<td><img src=\"$1.png\" alt=\"$1\"></td>"
}
html_rowend()
{
	echo >&3 "</tr>"
}
html_end()
{
	echo >&3 "</table></body></html>"
}

t()
{
	in=$1; shift
	out=$1; shift
	time "$@" < "$in" > "$out"
	html "$out"
}

html_start
for i in amelia dxtfail base_concrete1a disabled floor_tile3a lift02 panel_ceil1a sunset rms; do
	html_rowstart "$i"

	html "$i".tga

	nvcompress "$i".tga "$i".dds
	html "$i".dds

	t "$i".tga "$i"-rand64-srgb.dds ./s2tc -c SRGB       -r 64
#	t "$i".tga "$i"-norand-srgb.dds ./s2tc -c SRGB       -r 0
	t "$i".tga "$i"-rand64-mrgb.dds ./s2tc -c SRGB_MIXED -r 64
#	t "$i".tga "$i"-norand-mrgb.dds ./s2tc -c SRGB_MIXED -r 0
	t "$i".tga "$i"-rand64-yuv.dds  ./s2tc -c YUV        -r 64
#	t "$i".tga "$i"-norand-yuv.dds  ./s2tc -c YUV        -r 0
	t "$i".tga "$i"-rand64.dds      ./s2tc -c RGB        -r 64
#	t "$i".tga "$i"-norand.dds      ./s2tc -c RGB        -r 0
	t "$i".tga "$i"-rand64-avg.dds  ./s2tc -c AVG        -r 64
#	t "$i".tga "$i"-norand-avg.dds  ./s2tc -c AVG        -r 0

	html_rowend
done
html_end
