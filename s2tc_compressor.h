#ifndef S2TC_COMPRESSOR_H
#define S2TC_COMPRESSOR_H

// note: this is a C header file!

#ifdef __cplusplus
extern "C" {
#endif

void rgb565_image(unsigned char *out, const unsigned char *rgba, int w, int h, int alpharange);

typedef enum
{
	DXT1,
	DXT3,
	DXT5
} DxtMode;
typedef enum
{
	RGB,
	YUV,
	SRGB,
	SRGB_MIXED,
	LAB,
	AVG,
	NORMALMAP
} ColorDistMode;

void s2tc_encode_block(unsigned char *out, const unsigned char *rgba, int iw, int w, int h, DxtMode dxt, ColorDistMode cd, int nrandom);

#ifdef __cplusplus
}
#endif

#endif
