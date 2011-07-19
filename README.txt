S2TC Environment Variables
==========================

Color Distance Function
-----------------------
The following color distance functions can be selected by setting the
environment variable `S2TC_COLORDIST_MODE`:

*   `RGB`: weighted YCbCr difference assuming linear input
*   `YUV`: weighted Y'Cb'Cr' difference assuming sRGB input
*   `SRGB`: weighted YCbCr difference assuming sRGB input
*   `SRGB_MIXED`: weighted "Y'(Y)Cb'Cr'" difference assuming sRGB input
*   `AVG`: standard average of component difference (generic)
*   `WAVG`: weighted RGB average of component difference
*   `NORMALMAP`: vector distance after normalization

The default is `WAVG`, which is a good compromise between speed and quality for
RGB and sRGB data. For optimum quality on sRGB input, try `SRGB_MIXED`.

The color distance function defines how "closeness" of pixel values is judged
when the pixel color values are evaluated, colors are selected, or during
refinement.

Color Selection
---------------
The environment variable `S2TC_RANDOM_COLORS` can be set the following way:

*   `-1`: quick selection (darkest and brightest color are chosen, which is
    similar to the method in the Color Cell Compression paper)
*   `0`: all 16 input colors of a block are considered
*   greater than `0`: additionally, that many random color values in the range
    of the input color values are considered

The default is `-1`, which is fast but poor quality, however ideally suited for
online compression. For optimum quality, try `64`.

A bad color selection can later be compensated for by color refinement.

Color Refinement
----------------
The environment variable `S2TC_REFINE_COLORS` can be set to the following values:

*   `NEVER`: never run color refinement
*   `ALWAYS`: unconditionally perform color refinement
*   `LOOP`: perform color refinement, evaluate its output and discard it if it
    didn't improve quality, re-evaluate the pixel color values, and repeat
    until no improvement could be made

The default is `ALWAYS`, which is fast and decent quality, and usually doesn't
make things worse. For optimum quality, try `LOOP`.

Color refinement recalculates the color palette of a block after the pixel
value decision by averaging the color values of those encoded as c0 or c1, and
is a technique that helps a lot of the initial color selection was poor (e.g.
if `S2TC_RANDOM_COLORS` was not set, or set to `-1`).
