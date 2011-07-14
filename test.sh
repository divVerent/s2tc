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

	if $use_compressonator; then
		echo >&3 "<th>Compressonator</th>"
	fi
	if $use_nvcompress; then
		echo >&3 "<th>nvcompress</th>"
	fi
	echo >&3 "<th>rand64-sRGB-mixed</th>"
	echo >&3 "<th>rand64-wavg</th>"
	echo >&3 "<th>rand64-avg</th>"

	if $use_libtxc_dxtn; then
		echo >&3 "<th>libtxc_dxtn</th>"
	fi
	echo >&3 "<th>norand-wavg</th>"
	echo >&3 "<th>faster-wavg</th>"

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

if which nvcompress >/dev/null 2>&1; then
	use_nvcompress=true
else
	use_nvcompress=false
fi
if which wine >/dev/null 2>&1 && [ -f "$HOME/.wine/drive_c/Program Files (x86)/AMD/The Compressonator 1.50/TheCompressonator.exe" ]; then
	use_compressonator=true
else
	use_compressonator=false
fi
if [ -f /usr/lib/libtxc_dxtn.so ]; then
	use_libtxc_dxtn=true
else
	use_libtxc_dxtn=false
fi

html_start
for i in dxtfail base_concrete1a disabled floor_tile3a lift02 panel_ceil1a sunset amelia rms noise noise_solid supernova ishihara augenkrebs; do
	html_rowstart "$i"

	html "$i".tga

	if $use_compressonator; then
		time wine "c:/Program Files (x86)/AMD/The Compressonator 1.50/TheCompressonator.exe" -convert -overwrite -mipmaps "$i".tga "$i"-amdcompress.dds -codec DXTC.dll +fourCC DXT1 -mipper BoxFilter.dll
		html "$i"-amdcompress.dds
	fi

	if $use_nvcompress; then
		time nvcompress "$i".tga "$i"-nvcompress.dds
		html "$i"-nvcompress.dds
	fi

	( S2TC_COLORDIST_MODE=SRGB_MIXED S2TC_RANDOM_COLORS=64 S2TC_REFINE_COLORS=CHECK  t "$i".tga "$i"-rand64-mrgb-r.dds ./s2tc )
	( S2TC_COLORDIST_MODE=WAVG       S2TC_RANDOM_COLORS=64 S2TC_REFINE_COLORS=CHECK  t "$i".tga "$i"-rand64-wavg-r.dds ./s2tc )
	( S2TC_COLORDIST_MODE=AVG        S2TC_RANDOM_COLORS=64 S2TC_REFINE_COLORS=CHECK  t "$i".tga "$i"-rand64-avg-r.dds  ./s2tc )

	if $use_libtxc_dxtn; then
		( LD_PRELOAD=/usr/lib/libtxc_dxtn.so                                t "$i".tga "$i"-libtxc_dxtn.dds ./s2tc )
	fi
	( S2TC_COLORDIST_MODE=WAVG       S2TC_RANDOM_COLORS=0  S2TC_REFINE_COLORS=ALWAYS t "$i".tga "$i"-norand-wavg-r.dds ./s2tc )
	( S2TC_COLORDIST_MODE=WAVG       S2TC_RANDOM_COLORS=-1 S2TC_REFINE_COLORS=ALWAYS t "$i".tga "$i"-faster-wavg-r.dds ./s2tc )

	html_rowend
done
html_end
