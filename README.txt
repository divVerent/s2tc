S2TC - Texture Compression using Color Cell Compression


What is S2TC?

S2TC is a texture compression scheme that encodes 4x4 blocks with a 2 color
palette each. The storage format is chosen so that most current graphics cards
can decode it without any special updates or shaders; however, the format is
simpler and thus can allow for quicker online compression, or simpler decoding.

S2TC textures are stored in a DDS (DirectDraw Surface) container. After the 128
header bytes, alpha blocks and color blocks may follow. Blocks are encoded top
to bottom, left to right.

Notation:

When nothing else is specified, pixels are ordered from top to bottom, then
left to right. Bits are read from the least to the most significant bit. Larger
than a byte values are stored little-endian.

Subformats:

DXT1:

No alpha blocks are stored. Each color block starts with two 16-bit words c0,
c1. The most significant 5 bits encode red, the least significant bits encode
blue, the remaining 6 bits encode green. It must always be that c1 >= c0.

The remaining 32 bits are a little endian longword, of which each two adjacent
bits, encode one of the 16 pixels of the block. The two bits for a pixel can
encode the following values:

00 - color c0, alpha 255 (opaque)
01 - color c1, alpha 255 (opaque)
10 - reserved
11 - no color, alpha 0 (transparent)

DXT3:

Before each color block, an alpha block is stored. The alpha block is a 64bit
qword, of which each four adjacent bits encode one pixel value of alpha in
standard 4bpp encoding.

Each color block starts with two 16-bit words c0, c1. The most significant 5
bits encode red, the least significant bits encode blue, the remaining 6 bits
encode green. It must always be that c0 >= c1.

The remaining 32 bits are a little endian dword, of which each two adjacent
bits encode one of the 16 pixels of the block. The two bits for a pixel can
encode the following values:

00 - color c0
01 - color c1
10 - reserved
11 - reserved

DXT5:

Before each color block, an alpha block is stored. The alpha block starts with
two bytes for alpha values a0 and a1. It must always be that a1 >= a0. Then a
48-bit integer is encoded, of which each three adjacent bits encode one of the
16 alpha values of the block.

000 - alpha a0
001 - alpha a1
010 - reserved
011 - reserved
100 - reserved
101 - reserved
110 - alpha 0 (transparent)
111 - alpha 255 (opaque)

Each color block starts with two 16-bit words c0, c1. The most significant 5
bits encode red, the least significant bits encode blue, the remaining 6 bits
encode green. It must always be that c0 >= c1.

The remaining 32 bits are a little endian longword, of which each two adjacent
bits encode one of the 16 pixels of the block. The two bits for a pixel can
encode the following values:

00 - color c0
01 - color c1
10 - reserved
11 - reserved


Environment variables:

Color distance function: S2TC_COLORDIST_MODE (default: WAVG)
RGB:		weighted YCbCr difference assuming linear input
YUV:		weighted Y'Cb'Cr' difference assuming sRGB input
SRGB:		weighted YCbCr difference assuming sRGB input
SRGB_MIXED:	weighted "Y'(Y)Cb'Cr'" difference assuming sRGB input
LAB:		L*a*b* difference
AVG:		standard average of component difference (generic)
WAVG:		weighted RGB average of component difference
NORMALMAP: 	vector distance after normalization

Color selection: S2TC_RANDOM_COLORS (default: -1)
-1:	quick selection (just darkest and brightest color of each block)
0:	normal selection (all 16 colors of each block are candidates)
>0:	randomized selection (S2TC_RANDOM_COLORS random colors are added to the
	>candidates for each block)

Color refinement: S2TC_REFINE_COLORS (default: ALWAYS)
NEVER:	never run color refinement
ALWAYS:	always run color refinement (i.e. replace the colors by possibly better
	colors by averaging the original colors for the encoded pixels)
CHECK:	always run color refinement, but only use its result if it is actually
	a closer match for the block
LOOP:	always run color refinement, but only use its result if it is actually
	a closer match for the block; after refining, reassign colors and refine
	till no change for the better happened
