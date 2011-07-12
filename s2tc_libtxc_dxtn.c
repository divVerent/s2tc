#include "s2tc_compressor.h"

#include <GL/gl.h>
#include <stdlib.h>
#include <stdio.h>

void fetch_2d_texel_rgb_dxt1(GLint srcRowStride, const GLubyte *pixdata,
			     GLint i, GLint j, GLvoid *texel)
{
	// fetches a single texel (i,j) into pixdata (RGB)
	// TODO
}

void fetch_2d_texel_rgba_dxt1(GLint srcRowStride, const GLubyte *pixdata,
			     GLint i, GLint j, GLvoid *texel)
{
	// fetches a single texel (i,j) into pixdata (RGBA)
	// TODO
}

void fetch_2d_texel_rgba_dxt3(GLint srcRowStride, const GLubyte *pixdata,
			     GLint i, GLint j, GLvoid *texel)
{
	// fetches a single texel (i,j) into pixdata (RGBA)
	// TODO
}

void fetch_2d_texel_rgba_dxt5(GLint srcRowStride, const GLubyte *pixdata,
			     GLint i, GLint j, GLvoid *texel)
{
	// fetches a single texel (i,j) into pixdata (RGBA)
	// TODO
}

void tx_compress_dxtn(GLint srccomps, GLint width, GLint height,
		      const GLubyte *srcPixData, GLenum destFormat,
		      GLubyte *dest, GLint dstRowStride)
{
	// compresses width*height pixels (RGB or RGBA depending on srccomps) at srcPixData (packed) to destformat (dest, dstRowStride)

	GLubyte *blkaddr = dest;
	GLint numxpixels, numypixels;
	GLint i, j;
	GLint dstRowDiff;
	unsigned char *rgba = malloc(width * height * 4);
	unsigned char *srcaddr;

	switch (destFormat) {
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			rgb565_image(rgba, srcPixData, width, height, srccomps, 0, 1);
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
			rgb565_image(rgba, srcPixData, width, height, srccomps, 0, 15);
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			rgb565_image(rgba, srcPixData, width, height, srccomps, 0, 255);
			break;
		default:
			free(rgba);
			fprintf(stderr, "libdxtn: Bad dstFormat %d in tx_compress_dxtn\n", destFormat);
			return;
	}

	switch (destFormat) {
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			/* hmm we used to get called without dstRowStride... */
			dstRowDiff = dstRowStride >= (width * 2) ? dstRowStride - (((width + 3) & ~3) * 2) : 0;
			/*      fprintf(stderr, "dxt1 tex width %d tex height %d dstRowStride %d\n",
				width, height, dstRowStride); */
			for (j = 0; j < height; j += 4) {
				if (height > j + 3) numypixels = 4;
				else numypixels = height - j;
				srcaddr = rgba + j * width * 4;
				for (i = 0; i < width; i += 4) {
					if (width > i + 3) numxpixels = 4;
					else numxpixels = width - i;
					s2tc_encode_block(blkaddr, srcaddr, width, numxpixels, numypixels, DXT1, SRGB_MIXED, 0);
					srcaddr += 4 * numxpixels;
					blkaddr += 8;
				}
				blkaddr += dstRowDiff;
			}
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
			dstRowDiff = dstRowStride >= (width * 4) ? dstRowStride - (((width + 3) & ~3) * 4) : 0;
			/*      fprintf(stderr, "dxt3 tex width %d tex height %d dstRowStride %d\n",
				width, height, dstRowStride); */
			for (j = 0; j < height; j += 4) {
				if (height > j + 3) numypixels = 4;
				else numypixels = height - j;
				srcaddr = rgba + j * width * 4;
				for (i = 0; i < width; i += 4) {
					if (width > i + 3) numxpixels = 4;
					else numxpixels = width - i;
					s2tc_encode_block(blkaddr, srcaddr, width, numxpixels, numypixels, DXT3, SRGB_MIXED, 0);
					srcaddr += 4 * numxpixels;
					blkaddr += 16;
				}
				blkaddr += dstRowDiff;
			}
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			dstRowDiff = dstRowStride >= (width * 4) ? dstRowStride - (((width + 3) & ~3) * 4) : 0;
			/*      fprintf(stderr, "dxt5 tex width %d tex height %d dstRowStride %d\n",
				width, height, dstRowStride); */
			for (j = 0; j < height; j += 4) {
				if (height > j + 3) numypixels = 4;
				else numypixels = height - j;
				srcaddr = rgba + j * width * 4;
				for (i = 0; i < width; i += 4) {
					if (width > i + 3) numxpixels = 4;
					else numxpixels = width - i;
					s2tc_encode_block(blkaddr, srcaddr, width, numxpixels, numypixels, DXT5, SRGB_MIXED, 0);
					srcaddr += 4 * numxpixels;
					blkaddr += 16;
				}
				blkaddr += dstRowDiff;
			}
			break;
	}

	free(rgba);
}
