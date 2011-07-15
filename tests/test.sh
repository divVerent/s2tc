#!/bin/sh

set -e

cd ..
git clean -xdf
sh autogen.sh
./configure --prefix="`pwd`/tests" CXXFLAGS="-O3"
make
make install
cd tests

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
	echo >&3 "<th>rand32-sRGB-mixed</th>"
	echo >&3 "<th>rand32-wavg</th>"
	echo >&3 "<th>rand32-avg</th>"

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
	deltatime=
	deltatime_raw=0
	col=0
}

decompress()
{
	case "$1" in
		*.dds)
			convert "$1" TGA:-
			;;
		*)
			cat "$1"
			;;
	esac
}

html()
{
	decompress "$1" | convert TGA:- -crop 256x256+192+128 "html/$1.png"
	echo >&3 "<td><img src=\"$1.png\" alt=\"$1\" title=\"$1$deltatime\"></td>"
	eval "prevdeltatime=\$deltatime_$col"
	prevdeltatime=`echo "($prevdeltatime-0)+$deltatime_raw" | bc`
	eval "deltatime_$col=\$prevdeltatime"
	col=$(($col+1))
}
html2()
{
	bin/s2tc_decompress < "$1" | convert TGA:- -crop 256x256+192+128 "html/$1-s2tc.png"
	echo >&3 "<td><img src=\"$1-s2tc.png\" alt=\"$1\" title=\"$1$deltatime\"></td>"
	eval "prevdeltatime=\$deltatime_$col"
	prevdeltatime=`echo "($prevdeltatime-0)+$deltatime_raw" | bc`
	eval "deltatime_$col=\$prevdeltatime"
	col=$(($col+1))
}
html_rowend()
{
	echo >&3 "</tr>"
}
html_end()
{
	echo >&3 "<tr><th>Total runtime</th><td>(original)</td>"
	col=1
	while :; do
		eval "prevdeltatime=\$deltatime_$col"
		[ -n "$prevdeltatime" ] || break
		deltatime=`echo "scale=3; $prevdeltatime / 1000000000" | bc -l`
		echo >&3 "<td>$deltatime seconds</td>"
		col=$(($col+1))
	done
	echo >&3 "</table></body></html>"
}

timing()
{
	t0=`date +%s%N`
	"$@"
	t1=`date +%s%N`
	deltatime_raw=`echo "$t1 - $t0" | bc`
	deltatime=`echo "scale=3; $deltatime_raw / 1000000000" | bc -l`
	deltatime=" ($deltatime seconds)"
}

t()
{
	in=$1; shift
	out=$1; shift
	timing "$@" < "$in" > "$out"
	html "$out"
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

# TODO download test pictures that are not under the same license as this package

for i in dxtfail floor_tread01 floor_tread01_norm fract001 base_concrete1a disabled floor_tile3a lift02 panel_ceil1a sunset amelia rms noise noise_solid supernova ishihara augenkrebs; do
	html_rowstart "$i"

	html "$i".tga

	case "$i" in
		*_norm)
			fourcc=DXT5
			nvopts="-bc3 -alpha"
			;;
		*)
			fourcc=DXT1
			nvopts="-bc1"
			;;
	esac

	if $use_compressonator; then
		timing wine "c:/Program Files (x86)/AMD/The Compressonator 1.50/TheCompressonator.exe" -convert -overwrite -mipmaps "$i".tga "$i"-amdcompress.dds -codec DXTC.dll +fourCC $fourcc -mipper BoxFilter.dll
		html "$i"-amdcompress.dds
	fi

	if $use_nvcompress; then
		timing nvcompress $nvopts "$i".tga "$i"-nvcompress.dds
		html "$i"-nvcompress.dds
	fi

	S2TC_COLORDIST_MODE=SRGB_MIXED S2TC_RANDOM_COLORS=32 S2TC_REFINE_COLORS=CHECK  t "$i".tga "$i"-rand32-mrgb-r.dds bin/s2tc -t $fourcc
	S2TC_COLORDIST_MODE=WAVG       S2TC_RANDOM_COLORS=32 S2TC_REFINE_COLORS=CHECK  t "$i".tga "$i"-rand32-wavg-r.dds bin/s2tc -t $fourcc
	S2TC_COLORDIST_MODE=AVG        S2TC_RANDOM_COLORS=32 S2TC_REFINE_COLORS=CHECK  t "$i".tga "$i"-rand32-avg-r.dds  bin/s2tc -t $fourcc
	if $use_libtxc_dxtn; then
		LD_PRELOAD=/usr/lib/libtxc_dxtn.so                                     t "$i".tga "$i"-libtxc_dxtn.dds   bin/s2tc -t $fourcc
		unset LD_PRELOAD
	fi
	S2TC_COLORDIST_MODE=WAVG       S2TC_RANDOM_COLORS=0  S2TC_REFINE_COLORS=ALWAYS t "$i".tga "$i"-norand-wavg-r.dds bin/s2tc -t $fourcc
	S2TC_COLORDIST_MODE=WAVG       S2TC_RANDOM_COLORS=-1 S2TC_REFINE_COLORS=ALWAYS t "$i".tga "$i"-faster-wavg-r.dds bin/s2tc -t $fourcc

	html_rowend
done
html_end
