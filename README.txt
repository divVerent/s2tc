Environment variables:

Color distance function: S2TC_COLORDIST_MODE (default: WAVG)
RGB:		weighted YCbCr difference assuming linear input
YUV:		weighted Y'Cb'Cr' difference assuming sRGB input
SRGB:		weighted YCbCr difference assuming sRGB input
SRGB_MIXED:	weighted "Y'CbCr" difference assuming sRGB input
LAB:		L*a*b* difference
AVG:		standard average (generic)
WAVG:		weighted RGB average
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
