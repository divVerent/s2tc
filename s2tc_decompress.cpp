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
#define S2TC_LICENSE_IDENTIFIER s2tc_decompress_license
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
	typedef void (fetch_2d_texel_rgb_dxt1_t)(GLint srcRowStride, const GLubyte *pixdata,
			     GLint i, GLint j, GLvoid *texel);
	typedef void (fetch_2d_texel_rgba_dxt1_t)(GLint srcRowStride, const GLubyte *pixdata,
			     GLint i, GLint j, GLvoid *texel);
	typedef void (fetch_2d_texel_rgba_dxt3_t)(GLint srcRowStride, const GLubyte *pixdata,
			     GLint i, GLint j, GLvoid *texel);
	typedef void (fetch_2d_texel_rgba_dxt5_t)(GLint srcRowStride, const GLubyte *pixdata,
				     GLint i, GLint j, GLvoid *texel);
};
fetch_2d_texel_rgb_dxt1_t *fetch_2d_texel_rgb_dxt1 = NULL;
fetch_2d_texel_rgba_dxt1_t *fetch_2d_texel_rgba_dxt1 = NULL;
fetch_2d_texel_rgba_dxt3_t *fetch_2d_texel_rgba_dxt3 = NULL;
fetch_2d_texel_rgba_dxt5_t *fetch_2d_texel_rgba_dxt5 = NULL;
inline bool load_libraries(const char *n)
{
	void *l = dlopen(n, RTLD_NOW);
	if(!l)
	{
		fprintf(stderr, "Cannot load library: %s\n", dlerror());
		return false;
	}
	fetch_2d_texel_rgb_dxt1 = (fetch_2d_texel_rgb_dxt1_t *) dlsym(l, "fetch_2d_texel_rgb_dxt1");
	fetch_2d_texel_rgba_dxt1 = (fetch_2d_texel_rgba_dxt1_t *) dlsym(l, "fetch_2d_texel_rgba_dxt1");
	fetch_2d_texel_rgba_dxt3 = (fetch_2d_texel_rgba_dxt3_t *) dlsym(l, "fetch_2d_texel_rgba_dxt3");
	fetch_2d_texel_rgba_dxt5 = (fetch_2d_texel_rgba_dxt5_t *) dlsym(l, "fetch_2d_texel_rgba_dxt5");
	if(!fetch_2d_texel_rgb_dxt1 || !fetch_2d_texel_rgba_dxt1 || !fetch_2d_texel_rgba_dxt3 || !fetch_2d_texel_rgba_dxt5)
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
#ifdef ENABLE_RUNTIME_LINKING
			"    [-l path_to_libtxc_dxtn.so]\n"
#endif
			,
			me);
	return 1;
}

int main(int argc, char **argv)
{
	const char *infile = NULL, *outfile = NULL;

#ifdef ENABLE_RUNTIME_LINKING
	const char *library = "libtxc_dxtn.so";
#endif

	int opt;
	while((opt = getopt(argc, argv, "i:o:"
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

	FILE *infh = outfile ? fopen(outfile, "rb") : stdin;
	if(!infh)
	{
		printf("opening input failed\n");
		return 2;
	}

	FILE *outfh = outfile ? fopen(outfile, "wb") : stdout;
	if(!outfh)
	{
		printf("opening output failed\n");
		return 2;
	}

	uint32_t h[32];
	fread(h, sizeof(h), 1, infh);
	int height = LittleLong(h[3]);
	int width = LittleLong(h[4]);

	void (*fetch)(GLint srcRowStride, const GLubyte *pixdata, GLint i, GLint j, GLvoid *texel) = NULL;
	int fourcc = LittleLong(h[21]);
	int blocksize;
	switch(fourcc)
	{
		case 0x31545844:
			fetch = fetch_2d_texel_rgba_dxt1;
			blocksize = 8;
			break;
		case 0x33545844:
			fetch = fetch_2d_texel_rgba_dxt3;
			blocksize = 16;
			break;
		case 0x35545844:
			fetch = fetch_2d_texel_rgba_dxt5;
			blocksize = 16;
			break;
		default:
			fprintf(stderr, "Only DXT1, DXT3, DXT5 are supported!\n");
			return 1;
	}

	unsigned char t[18];
	memset(t, 0, 18);
	t[2]  = 2;
	t[12] = width % 256;
	t[13] = width / 256;
	t[14] = height % 256;
	t[15] = height / 256;
	t[16] = 32;
	t[17] = 0x28;
	fwrite(t, 18, 1, outfh);

	int n = ((width + 3) / 4) * ((height + 3) / 4);
	unsigned char *buf = (unsigned char *) malloc(n * blocksize);
	fread(buf, blocksize, n, infh);

	int x, y;
	for(y = 0; y < height; ++y)
		for(x = 0; x < width; ++x)
		{
			char data[4];
			fetch(width, buf, x, y, &data);
			std::swap(data[0], data[2]);
			fwrite(data, 4, 1, outfh);
		}

	if(infile)
		fclose(infh);
	if(outfile)
		fclose(outfh);

	return 0;
}
