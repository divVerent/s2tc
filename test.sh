#!/bin/sh

set -ex

CXX="g++ -Wall -Wextra -O3"
make clean all

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
	echo >&3 "<th>Compressonator</th>"
	echo >&3 "<th>nvcompress</th>"
	echo >&3 "<th>libtxc_dxtn</th>"
	echo >&3 "<th>rand64-sRGB-mixed</th>"
	echo >&3 "<th>rand64-wavg</th>"
	echo >&3 "<th>norand-wavg</th>"
	echo >&3 "<th>faster-wavg</th>"
	echo >&3 "<th>norand-avg</th>"
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
html2()
{
	./s2tc_decompress < "$1" | convert TGA:- -crop 256x256+192+128 "html/$1-s2tc.png"
	echo >&3 "<td><img src=\"$1-s2tc.png\" alt=\"$1\"></td>"
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
	echo "$LD_PRELOAD"
}

html_start
for i in dxtfail base_concrete1a disabled floor_tile3a lift02 panel_ceil1a sunset amelia rms noise noise_solid supernova ishihara augenkrebs; do
	html_rowstart "$i"

	html "$i".tga

	time wine "c:/Program Files (x86)/AMD/The Compressonator 1.50/TheCompressonator.exe" -convert -overwrite -mipmaps "$i".tga "$i"_amdcompress.dds -codec ATIC.dll +fourCC DXT1 -mipper BoxFilter.dll
	html "$i"_amdcompress.dds

	time nvcompress "$i".tga "$i"_nvcompress.dds
	html "$i"_nvcompress.dds

	export LD_PRELOAD=/usr/lib/libtxc_dxtn.so
	                                                     t "$i".tga "$i"-libtxc_dxtn.dds ./s2tc
	unset LD_PRELOAD
	S2TC_COLORDIST_MODE=SRGB_MIXED S2TC_RANDOM_COLORS=64 t "$i".tga "$i"-rand64-mrgb.dds ./s2tc
	S2TC_COLORDIST_MODE=WAVG       S2TC_RANDOM_COLORS=64 t "$i".tga "$i"-rand64-wavg.dds ./s2tc
	S2TC_COLORDIST_MODE=WAVG       S2TC_RANDOM_COLORS=0  t "$i".tga "$i"-norand-wavg.dds ./s2tc
	S2TC_COLORDIST_MODE=WAVG       S2TC_RANDOM_COLORS=-1 t "$i".tga "$i"-faster-wavg.dds ./s2tc
	S2TC_COLORDIST_MODE=AVG        S2TC_RANDOM_COLORS=0  t "$i".tga "$i"-norand-avg.dds  ./s2tc

	html_rowend
done
html_end
