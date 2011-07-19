/*
 * Copyright (C) 2011  Rudolf Polzer   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * RUDOLF POLZER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#define S2TC_LICENSE_IDENTIFIER s2tc_compress_license
#include "s2tc_license.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <algorithm>
#include "s2tc_common.h"

#ifdef ENABLE_RUNTIME_LINKING
#include <dlfcn.h>
#include <GL/gl.h>
extern "C"
{
	typedef void (tx_compress_dxtn_t)(GLint srccomps, GLint width, GLint height,
			      const GLubyte *srcPixData, GLenum destformat,
			      GLubyte *dest, GLint dstRowStride);
};
tx_compress_dxtn_t *tx_compress_dxtn = NULL;
inline bool load_libraries(const char *n)
{
	void *l = dlopen(n, RTLD_NOW);
	if(!l)
	{
		fprintf(stderr, "Cannot load library: %s\n", dlerror());
		return false;
	}
	tx_compress_dxtn = (tx_compress_dxtn_t *) dlsym(l, "tx_compress_dxtn");
	if(!tx_compress_dxtn)
	{
		fprintf(stderr, "The selected libtxc_dxtn.so does not contain all required symbols.");
		dlclose(l);
		return false;
	}
	return true;
}
#else
extern "C"
{
#include "txc_dxtn.h"
};
#endif

/* START stuff that originates from image.c in DarkPlaces */
/*
	Thu Jul 14 21:58:02 CEST 2011
	21:25:25        @divVerent | LordHavoc: http://paste.pocoo.org/show/438804/
	21:25:31        @divVerent | can I have this code under a MIT-style license?
	21:59:58        @LordHavoc | divVerent: yeah, have them under any license you want
	22:00:11        @LordHavoc | divVerent: my attitude toward licenses is generally "WTFPL would be preferably if I could use it" :P
	22:04:01        @divVerent | LordHavoc: okay, thanks
*/

int image_width, image_height;

typedef struct _TargaHeader
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
}
TargaHeader;

unsigned char *LoadTGA_BGRA (const unsigned char *f, int filesize)
{
	int x, y, pix_inc, row_inci, runlen, alphabits;
	unsigned char *image_buffer;
	unsigned int *pixbufi;
	const unsigned char *fin, *enddata;
	TargaHeader targa_header;
	unsigned int palettei[256];
	union
	{
		unsigned int i;
		unsigned char b[4];
	}
	bgra;

	if (filesize < 19)
		return NULL;

	enddata = f + filesize;

	targa_header.id_length = f[0];
	targa_header.colormap_type = f[1];
	targa_header.image_type = f[2];

	targa_header.colormap_index = f[3] + f[4] * 256;
	targa_header.colormap_length = f[5] + f[6] * 256;
	targa_header.colormap_size = f[7];
	targa_header.x_origin = f[8] + f[9] * 256;
	targa_header.y_origin = f[10] + f[11] * 256;
	targa_header.width = image_width = f[12] + f[13] * 256;
	targa_header.height = image_height = f[14] + f[15] * 256;
	targa_header.pixel_size = f[16];
	targa_header.attributes = f[17];

	if (image_width > 32768 || image_height > 32768 || image_width <= 0 || image_height <= 0)
	{
		printf("LoadTGA: invalid size\n");
		return NULL;
	}

	/* advance to end of header */
	fin = f + 18;

	/* skip TARGA image comment (usually 0 bytes) */
	fin += targa_header.id_length;

	/* read/skip the colormap if present (note: according to the TARGA spec it */
	/* can be present even on 1color or greyscale images, just not used by */
	/* the image data) */
	if (targa_header.colormap_type)
	{
		if (targa_header.colormap_length > 256)
		{
			printf("LoadTGA: only up to 256 colormap_length supported\n");
			return NULL;
		}
		if (targa_header.colormap_index)
		{
			printf("LoadTGA: colormap_index not supported\n");
			return NULL;
		}
		if (targa_header.colormap_size == 24)
		{
			for (x = 0;x < targa_header.colormap_length;x++)
			{
				bgra.b[0] = *fin++;
				bgra.b[1] = *fin++;
				bgra.b[2] = *fin++;
				bgra.b[3] = 255;
				palettei[x] = bgra.i;
			}
		}
		else if (targa_header.colormap_size == 32)
		{
			memcpy(palettei, fin, targa_header.colormap_length*4);
			fin += targa_header.colormap_length * 4;
		}
		else
		{
			printf("LoadTGA: Only 32 and 24 bit colormap_size supported\n");
			return NULL;
		}
	}

	/* check our pixel_size restrictions according to image_type */
	switch (targa_header.image_type & ~8)
	{
	case 2:
		if (targa_header.pixel_size != 24 && targa_header.pixel_size != 32)
		{
			printf("LoadTGA: only 24bit and 32bit pixel sizes supported for type 2 and type 10 images\n");
			return NULL;
		}
		break;
	case 3:
		/* set up a palette to make the loader easier */
		for (x = 0;x < 256;x++)
		{
			bgra.b[0] = bgra.b[1] = bgra.b[2] = x;
			bgra.b[3] = 255;
			palettei[x] = bgra.i;
		}
		/* fall through to colormap case */
	case 1:
		if (targa_header.pixel_size != 8)
		{
			printf("LoadTGA: only 8bit pixel size for type 1, 3, 9, and 11 images supported\n");
			return NULL;
		}
		break;
	default:
		printf("LoadTGA: Only type 1, 2, 3, 9, 10, and 11 targa RGB images supported, image_type = %i\n", targa_header.image_type);
		return NULL;
	}

	if (targa_header.attributes & 0x10)
	{
		printf("LoadTGA: origin must be in top left or bottom left, top right and bottom right are not supported\n");
		return NULL;
	}

	/* number of attribute bits per pixel, we only support 0 or 8 */
	alphabits = targa_header.attributes & 0x0F;
	if (alphabits != 8 && alphabits != 0)
	{
		printf("LoadTGA: only 0 or 8 attribute (alpha) bits supported\n");
		return NULL;
	}

	image_buffer = (unsigned char *)malloc(image_width * image_height * 4);
	if (!image_buffer)
	{
		printf("LoadTGA: not enough memory for %i by %i image\n", image_width, image_height);
		return NULL;
	}

	/* If bit 5 of attributes isn't set, the image has been stored from bottom to top */
	if ((targa_header.attributes & 0x20) == 0)
	{
		pixbufi = (unsigned int*)image_buffer + (image_height - 1)*image_width;
		row_inci = -image_width*2;
	}
	else
	{
		pixbufi = (unsigned int*)image_buffer;
		row_inci = 0;
	}

	x = 0;
	y = 0;
	pix_inc = 1;
	if ((targa_header.image_type & ~8) == 2)
		pix_inc = (targa_header.pixel_size + 7) / 8;
	switch (targa_header.image_type)
	{
	case 1: /* colormapped, uncompressed */
	case 3: /* greyscale, uncompressed */
		if (fin + image_width * image_height * pix_inc > enddata)
			break;
		for (y = 0;y < image_height;y++, pixbufi += row_inci)
			for (x = 0;x < image_width;x++)
				*pixbufi++ = palettei[*fin++];
		break;
	case 2:
		/* BGR or BGRA, uncompressed */
		if (fin + image_width * image_height * pix_inc > enddata)
			break;
		if (targa_header.pixel_size == 32 && alphabits)
		{
			for (y = 0;y < image_height;y++)
				memcpy(pixbufi + y * (image_width + row_inci), fin + y * image_width * pix_inc, image_width*4);
		}
		else
		{
			for (y = 0;y < image_height;y++, pixbufi += row_inci)
			{
				for (x = 0;x < image_width;x++, fin += pix_inc)
				{
					bgra.b[0] = fin[0];
					bgra.b[1] = fin[1];
					bgra.b[2] = fin[2];
					bgra.b[3] = 255;
					*pixbufi++ = bgra.i;
				}
			}
		}
		break;
	case 9: /* colormapped, RLE */
	case 11: /* greyscale, RLE */
		for (y = 0;y < image_height;y++, pixbufi += row_inci)
		{
			for (x = 0;x < image_width;)
			{
				if (fin >= enddata)
					break; /* error - truncated file */
				runlen = *fin++;
				if (runlen & 0x80)
				{
					/* RLE - all pixels the same color */
					runlen += 1 - 0x80;
					if (fin + pix_inc > enddata)
						break; /* error - truncated file */
					if (x + runlen > image_width)
						break; /* error - line exceeds width */
					bgra.i = palettei[*fin++];
					for (;runlen--;x++)
						*pixbufi++ = bgra.i;
				}
				else
				{
					/* uncompressed - all pixels different color */
					runlen++;
					if (fin + pix_inc * runlen > enddata)
						break; /* error - truncated file */
					if (x + runlen > image_width)
						break; /* error - line exceeds width */
					for (;runlen--;x++)
						*pixbufi++ = palettei[*fin++];
				}
			}

			if (x != image_width)
			{
				/* pixbufi is useless now */
				printf("LoadTGA: corrupt file\n");
				break;
			}
		}
		break;
	case 10:
		/* BGR or BGRA, RLE */
		if (targa_header.pixel_size == 32 && alphabits)
		{
			for (y = 0;y < image_height;y++, pixbufi += row_inci)
			{
				for (x = 0;x < image_width;)
				{
					if (fin >= enddata)
						break; /* error - truncated file */
					runlen = *fin++;
					if (runlen & 0x80)
					{
						/* RLE - all pixels the same color */
						runlen += 1 - 0x80;
						if (fin + pix_inc > enddata)
							break; /* error - truncated file */
						if (x + runlen > image_width)
							break; /* error - line exceeds width */
						bgra.b[0] = fin[0];
						bgra.b[1] = fin[1];
						bgra.b[2] = fin[2];
						bgra.b[3] = fin[3];
						fin += pix_inc;
						for (;runlen--;x++)
							*pixbufi++ = bgra.i;
					}
					else
					{
						/* uncompressed - all pixels different color */
						runlen++;
						if (fin + pix_inc * runlen > enddata)
							break; /* error - truncated file */
						if (x + runlen > image_width)
							break; /* error - line exceeds width */
						for (;runlen--;x++)
						{
							bgra.b[0] = fin[0];
							bgra.b[1] = fin[1];
							bgra.b[2] = fin[2];
							bgra.b[3] = fin[3];
							fin += pix_inc;
							*pixbufi++ = bgra.i;
						}
					}
				}

				if (x != image_width)
				{
					/* pixbufi is useless now */
					printf("LoadTGA: corrupt file\n");
					break;
				}
			}
		}
		else
		{
			for (y = 0;y < image_height;y++, pixbufi += row_inci)
			{
				for (x = 0;x < image_width;)
				{
					if (fin >= enddata)
						break; /* error - truncated file */
					runlen = *fin++;
					if (runlen & 0x80)
					{
						/* RLE - all pixels the same color */
						runlen += 1 - 0x80;
						if (fin + pix_inc > enddata)
							break; /* error - truncated file */
						if (x + runlen > image_width)
							break; /* error - line exceeds width */
						bgra.b[0] = fin[0];
						bgra.b[1] = fin[1];
						bgra.b[2] = fin[2];
						bgra.b[3] = 255;
						fin += pix_inc;
						for (;runlen--;x++)
							*pixbufi++ = bgra.i;
					}
					else
					{
						/* uncompressed - all pixels different color */
						runlen++;
						if (fin + pix_inc * runlen > enddata)
							break; /* error - truncated file */
						if (x + runlen > image_width)
							break; /* error - line exceeds width */
						for (;runlen--;x++)
						{
							bgra.b[0] = fin[0];
							bgra.b[1] = fin[1];
							bgra.b[2] = fin[2];
							bgra.b[3] = 255;
							fin += pix_inc;
							*pixbufi++ = bgra.i;
						}
					}
				}

				if (x != image_width)
				{
					/* pixbufi is useless now */
					printf("LoadTGA: corrupt file\n");
					break;
				}
			}
		}
		break;
	default:
		/* unknown image_type */
		break;
	}

	return image_buffer;
}

// in can be the same as out
void Image_MipReduce32(const unsigned char *in, unsigned char *out, int *width, int *height, int destwidth, int destheight)
{
	const unsigned char *inrow;
	int x, y, nextrow;
	// note: if given odd width/height this discards the last row/column of
	// pixels, rather than doing a proper box-filter scale down
	inrow = in;
	nextrow = *width * 4;
	if (*width > destwidth)
	{
		*width >>= 1;
		if (*height > destheight)
		{
			// reduce both
			*height >>= 1;
			for (y = 0;y < *height;y++, inrow += nextrow * 2)
			{
				for (in = inrow, x = 0;x < *width;x++)
				{
					out[0] = (unsigned char) ((in[0] + in[4] + in[nextrow  ] + in[nextrow+4]) >> 2);
					out[1] = (unsigned char) ((in[1] + in[5] + in[nextrow+1] + in[nextrow+5]) >> 2);
					out[2] = (unsigned char) ((in[2] + in[6] + in[nextrow+2] + in[nextrow+6]) >> 2);
					out[3] = (unsigned char) ((in[3] + in[7] + in[nextrow+3] + in[nextrow+7]) >> 2);
					out += 4;
					in += 8;
				}
			}
		}
		else
		{
			// reduce width
			for (y = 0;y < *height;y++, inrow += nextrow)
			{
				for (in = inrow, x = 0;x < *width;x++)
				{
					out[0] = (unsigned char) ((in[0] + in[4]) >> 1);
					out[1] = (unsigned char) ((in[1] + in[5]) >> 1);
					out[2] = (unsigned char) ((in[2] + in[6]) >> 1);
					out[3] = (unsigned char) ((in[3] + in[7]) >> 1);
					out += 4;
					in += 8;
				}
			}
		}
	}
	else
	{
		if (*height > destheight)
		{
			// reduce height
			*height >>= 1;
			for (y = 0;y < *height;y++, inrow += nextrow * 2)
			{
				for (in = inrow, x = 0;x < *width;x++)
				{
					out[0] = (unsigned char) ((in[0] + in[nextrow  ]) >> 1);
					out[1] = (unsigned char) ((in[1] + in[nextrow+1]) >> 1);
					out[2] = (unsigned char) ((in[2] + in[nextrow+2]) >> 1);
					out[3] = (unsigned char) ((in[3] + in[nextrow+3]) >> 1);
					out += 4;
					in += 4;
				}
			}
		}
	}
}
unsigned char *FS_LoadFile(const char *fn, int *len)
{
	unsigned char *buf = NULL;
	int n;
	FILE *f = fn ? fopen(fn, "rb") : stdin;
	*len = 0;
	if(!f)
		return NULL;
	for(;;)
	{
		buf = (unsigned char *) realloc(buf, *len + 65536);
		if(!buf)
		{
			if(fn)
				fclose(f);
			free(buf);
			*len = 0;
			return NULL;
		}
		n = fread(buf + *len, 1, 65536, f);
		if(n < 0)
		{
			if(fn)
				fclose(f);
			free(buf);
			*len = 0;
			return NULL;
		}
		*len += n;
		if(n < 65536)
			break;
	}
	if(fn)
		fclose(f);
	return buf;
}
/* END of darkplaces stuff */

uint32_t LittleLong(uint32_t w)
{
	union
	{
		unsigned char c[4];
		uint32_t u;
	}
	un;
	un.c[0] = w;
	un.c[1] = w >> 8;
	un.c[2] = w >> 16;
	un.c[3] = w >> 24;
	return un.u;
}

int usage(const char *me)
{
	fprintf(stderr, "usage:\n"
			"%s \n"
			"    [-i infile.tga]\n"
			"    [-o outfile.dds]\n"
			"    [-t {DXT1|DXT3|DXT5}]\n"
#ifdef ENABLE_RUNTIME_LINKING
			"    [-l path_to_libtxc_dxtn.so]\n"
#endif
			,
			me);
	return 1;
}

int main(int argc, char **argv)
{
	GLenum dxt = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
	const char *infile = NULL, *outfile = NULL;

#ifdef ENABLE_RUNTIME_LINKING
	const char *library = "libtxc_dxtn.so";
#endif

	int opt;
	while((opt = getopt(argc, argv, "i:o:t:"
#ifdef ENABLE_RUNTIME_LINKING
					"l:"
#endif
					)) != -1)
	{
		switch(opt)
		{
			case 'i':
				infile = optarg;
				break;
			case 'o':
				outfile = optarg;
				break;
			case 't':
				if(!strcasecmp(optarg, "DXT1"))
					dxt = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
				else if(!strcasecmp(optarg, "DXT3"))
					dxt = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
				else if(!strcasecmp(optarg, "DXT5"))
					dxt = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				else
					return usage(argv[0]);
				break;
#ifdef ENABLE_RUNTIME_LINKING
			case 'l':
				library = optarg;
				break;
#endif
			default:
				return usage(argv[0]);
				break;
		}
	}
#ifdef ENABLE_RUNTIME_LINKING
	if(!load_libraries(library))
		return 1;
#endif

	FILE *outfh = outfile ? fopen(outfile, "wb") : stdout;
	if(!outfh)
	{
		printf("opening output failed\n");
		return 2;
	}

	int piclen;
	unsigned char *picdata = FS_LoadFile(infile, &piclen);
	if(!picdata)
	{
		printf("FS_LoadFile failed\n");
		return 2;
	}

	unsigned char *pic = LoadTGA_BGRA(picdata, piclen);
	for(int x = 0; x < image_width*image_height; ++x)
		std::swap(pic[4*x], pic[4*x+2]);
	int mipcount = 0;
	while(image_width >= (1 << mipcount) || image_height >= (1 << mipcount))
		++mipcount;
	// now, (1 << mipcount) >= width, height

	const char *fourcc;
	int blocksize;
	switch(dxt)
	{
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			blocksize = 8;
			fourcc = "DXT1";
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
			blocksize = 16;
			fourcc = "DXT3";
			break;
		default:
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			blocksize = 16;
			fourcc = "DXT5";
			break;
	}

	{
		bool alphapixels = false;
		for(int y = 0; y < image_height; ++y)
			for(int x = 0; x < image_width; ++x)
				if(pic[(y*image_width+x)*4+3] != 255)
				{
					alphapixels = true;
					break;
				}

		uint32_t zero = LittleLong(0);
		uint32_t dds_picsize = LittleLong(((image_width+3)/4) * ((image_height+3)/4) * blocksize);
		uint32_t dds_mipcount = LittleLong(mipcount);
		uint32_t dds_width = LittleLong(image_width);
		uint32_t dds_height = LittleLong(image_height);

		//0
		fwrite("DDS ", 4, 1, outfh);
		fwrite("\x7c\x00\x00\x00", 4, 1, outfh);
		fwrite("\x07\x10\x0a\x00", 4, 1, outfh);
		fwrite(&dds_height, 4, 1, outfh);
		fwrite(&dds_width, 4, 1, outfh);
		fwrite(&dds_picsize, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&dds_mipcount, 4, 1, outfh);

		//32
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);

		//64
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite("\x20\x00\x00\x00", 4, 1, outfh);
		fwrite(alphapixels ? "\x05\x00\x00\x00" : "\x04\x00\x00\x00", 4, 1, outfh);
		fwrite(fourcc, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);

		//96
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite("\x08\x10\x40\x00", 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
	}

	for(;;)
	{
		int blocks_w = (image_width + 3) / 4;
		int blocks_h = (image_height + 3) / 4;
		GLubyte *obuf = (GLubyte *) malloc(blocksize * blocks_w * blocks_h);
		tx_compress_dxtn(4, image_width, image_height, pic, dxt, obuf, blocks_w * blocksize);
		fwrite(obuf, blocksize * blocks_w * blocks_h, 1, outfh);
		free(obuf);
		if(image_width == 1 && image_height == 1)
			break;
		Image_MipReduce32(pic, pic, &image_width, &image_height, 1, 1);
	}

	if(outfile)
		fclose(outfh);

	return 0;
}
