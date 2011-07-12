#ifndef S2TC_COMPRESSOR_H
#define S2TC_COMPRESSOR_H

void rgb565_image(unsigned char *out, const unsigned char *rgba, int w, int h, int alpharange);

enum DxtMode
{
	DXT1,
	DXT3,
	DXT5
};
enum ColorDistMode
{
	RGB,
	YUV,
	SRGB,
	SRGB_MIXED,
	LAB,
	AVG,
	NORMALMAP
};

void s2tc_encode_block(unsigned char *out, const unsigned char *rgba, int iw, int w, int h, DxtMode dxt, ColorDistMode cd, int nrandom);

#endif
