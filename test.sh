#!/bin/sh

set -ex

CXX="g++ -Wall -Wextra -O3"
$CXX s2tc.cpp -o s2tc

mkdir -p html
exec 3>html/index.html

html_start()
{
	echo >&3 "<html><title>S2TC</title>"
	cat <<'EOF' >&3
<script type="text/javascript" src="http://code.jquery.com/jquery-1.6.2.min.js"></script>
<script type="text/javascript">
var refsrc = "";
function clickfunc()
{
	var me = $(this);
	if(!me.data("src"))
		me.data("src", me.attr("src"));
	me.attr("src", me.data("src"));
	if(refsrc == me.data("src"))
		refsrc = "";
	else
		refsrc = me.data("src");
}
function enterfunc()
{
	var me = $(this);
	if(!me.data("src"))
		me.data("src", me.attr("src"));
	if(refsrc != "")
		me.attr("src", refsrc);
}
function leavefunc()
{
	var me = $(this);
	if(me.data("src"))
		me.attr("src", me.data("src"));
}
function run()
{
	$('img').click(clickfunc);
	$('img').mouseenter(enterfunc);
	$('img').mouseleave(leavefunc);
}
</script>
EOF
	echo >&3 "<body onLoad=\"run()\"><h1>S2TC</h1>"
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
	convert "$1" -crop 256x256+192+128 "html/$1.png"
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
	[ -f "$out" ] || time "$@" < "$in" > "$out"
	html "$out"
}

html_start
for i in dxtfail base_concrete1a disabled floor_tile3a lift02 panel_ceil1a sunset amelia rms noise noise_solid supernova ishihara augenkrebs; do
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
