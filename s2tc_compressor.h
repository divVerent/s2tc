#ifndef S2TC_COMPRESSOR_H
#define S2TC_COMPRESSOR_H

// note: this is a C header file!

#ifdef __cplusplus
extern "C" {
#endif

void rgb565_image(unsigned char *out, const unsigned char *rgba, int w, int h, int srccomps, int bgr, int alphabits);

enum DxtMode
{
	DXT1,
	DXT3,
	DXT5
};
enum RefinementMode
{
	REFINE_NEVER,
	REFINE_ALWAYS,
	REFINE_CHECK,
	REFINE_LOOP
};

typedef enum
{
	RGB,
	YUV,
	SRGB,
	SRGB_MIXED,
	LAB,
	AVG,
	WAVG,
	NORMALMAP
} ColorDistMode;

typedef void (*s2tc_encode_block_func_t) (unsigned char *out, const unsigned char *rgba, int iw, int w, int h, int nrandom);
s2tc_encode_block_func_t s2tc_encode_block_func(DxtMode dxt, ColorDistMode cd, int nrandom, RefinementMode refine);

#ifdef __cplusplus
}
#endif

#endif
