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
#define S2TC_LICENSE_IDENTIFIER s2tc_from_s3tc_license
#include "s2tc_license.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <algorithm>
#include "s2tc_common.h"

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
			"    [-i infile.dds]\n"
			"    [-o outfile.dds]\n"
			,
			me);
	return 1;
}

enum DxtConversion
{
	DXT1,
	DXT3,
	DXT5
};

//pixels = (pixels & ~((~pixels & 0x55555555) << 1)) | ((pixels & 0x22882288) >> 1);
// 00 -> 00
// 01 -> 01
// 10 -> 00 or 01
// 11 -> 11

//pixels = (pixels & ((~pixels & 0xAAAAAAAA) >> 1)) | ((pixels & 0x22882288) >> 1);
// 00 -> 00
// 01 -> 01
// 10 -> 00 or 01
// 11 -> 00 or 01

void convert_dxt1(unsigned char *buf)
{
	unsigned int c  = buf[0] + 256*buf[1];
	unsigned int c1 = buf[2] + 256*buf[3];
	uint32_t pixels = buf[4] | (((uint32_t)buf[5]) << 8) | (((uint32_t)buf[6]) << 16) | (((uint32_t)buf[7]) << 24);
	if(c1 >= c)
	{
		// we have 10, 11 "cannot be", but we better treat 11 the same way as 10 here
		pixels = (pixels & ((~pixels & 0xAAAAAAAA) >> 1)) | ((pixels & 0x22882288) >> 1);

		// S2TC conformance: always use the same order of c, c1
		// swap
		std::swap(c1, c);
		// invert
		pixels ^= 0x55555555;
	}
	else
	{
		// we have no alpha
		pixels = (pixels & ((~pixels & 0xAAAAAAAA) >> 1)) | ((pixels & 0x22882288) >> 1);
		// alternatively: collapse
		//pixels = pixels & 0x55555555;
	}
	buf[0] = c & 0xFF;
	buf[1] = c >> 8;
	buf[2] = c1 & 0xFF;
	buf[3] = c1 >> 8;
	buf[4] = pixels & 0xFF;
	buf[5] = (pixels >> 8) & 0xFF;
	buf[6] = (pixels >> 16) & 0xFF;
	buf[7] = (pixels >> 24) & 0xFF;
}

void convert_dxt1a(unsigned char *buf)
{
	unsigned int c  = buf[0] + 256*buf[1];
	unsigned int c1 = buf[2] + 256*buf[3];
	uint32_t pixels = buf[4] | (((uint32_t)buf[5]) << 8) | (((uint32_t)buf[6]) << 16) | (((uint32_t)buf[7]) << 24);
	if(c1 >= c)
	{
		// we have alpha, don't break it
		pixels = (pixels & ~((~pixels & 0x55555555) << 1)) | ((pixels & 0x22882288) >> 1);
	}
	else
	{
		// we have no alpha
		pixels = (pixels & ((~pixels & 0xAAAAAAAA) >> 1)) | ((pixels & 0x22882288) >> 1);
		// alternatively: collapse
		//pixels = pixels & 0x55555555;

		// S2TC conformance: always use the same order of c, c1
		// swap
		std::swap(c1, c);
		// invert
		pixels ^= 0x55555555;
	}
	buf[0] = c & 0xFF;
	buf[1] = c >> 8;
	buf[2] = c1 & 0xFF;
	buf[3] = c1 >> 8;
	buf[4] = pixels & 0xFF;
	buf[5] = (pixels >> 8) & 0xFF;
	buf[6] = (pixels >> 16) & 0xFF;
	buf[7] = (pixels >> 24) & 0xFF;
}

void convert_dxt5(unsigned char *buf)
{
	unsigned int a  = buf[0];
	unsigned int a1 = buf[1];
	uint64_t pixels = buf[2] | (((uint32_t)buf[3]) << 8) | (((uint32_t)buf[4]) << 16) | (((uint32_t)buf[5]) << 24) | (((uint64_t)buf[6]) << 32) | (((uint64_t)buf[7]) << 48);
	if(a1 >= a)
	{
		// we want to map:
		// 000 -> 000
		// 001 -> 001
		// 010 -> 000 or 001
		// 011 -> 000 or 001
		// 100 -> 001 or 000
		// 101 -> 001 or 000
		// 110 -> 110
		// 111 -> 111

		pixels = (pixels & ~((((pixels >> 1) ^ (pixels >> 2)) & 01111111111111111ull) * 7)) | ((((pixels >> 1) ^ (pixels >> 2)) & 00101010101010101ull) * 7);
	}
	else
	{
		// we want to map:
		// 000 -> 000
		// 001 -> 001
		// 010 -> 000 or 001
		// 011 -> 000 or 001
		// 100 -> 000 or 001
		// 101 -> 001 or 000
		// 110 -> 001 or 000
		// 111 -> 001 or 000

		pixels = (pixels & ~((((pixels >> 1) | (pixels >> 2)) & 01111111111111111ull) * 7)) | ((((pixels >> 1) | (pixels >> 2)) & 00101010101010101ull) * 7);

		// S2TC conformance: always use the same order of a, a1
		// swap
		std::swap(a1, a);
		// invert
		pixels ^= 01111111111111111ull;
	}
	buf[0] = a;
	buf[1] = a1;
	buf[2] = pixels & 0xFF;
	buf[3] = (pixels >> 8) & 0xFF;
	buf[4] = (pixels >> 16) & 0xFF;
	buf[5] = (pixels >> 24) & 0xFF;
	buf[6] = (pixels >> 32) & 0xFF;
	buf[7] = (pixels >> 40) & 0xFF;
}

int main(int argc, char **argv)
{
	const char *infile = NULL, *outfile = NULL;

	int opt;
	while((opt = getopt(argc, argv, "i:o:")) != -1)
	{
		switch(opt)
		{
			case 'i':
				infile = optarg;
				break;
			case 'o':
				outfile = optarg;
				break;
			default:
				return usage(argv[0]);
				break;
		}
	}

	FILE *infh = infile ? fopen(infile, "rb") : stdin;
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

	int fourcc = LittleLong(h[21]);
	int blocksize;
	DxtConversion mode;
	switch(fourcc)
	{
		case 0x31545844:
			blocksize = 8;
			mode = DXT1;
			break;
		case 0x33545844:
			blocksize = 16;
			mode = DXT3;
			break;
		case 0x35545844:
			blocksize = 16;
			mode = DXT5;
			break;
		default:
			fprintf(stderr, "Only DXT1, DXT3, DXT5 are supported!\n");
			return 1;
	}

	fwrite(h, sizeof(h), 1, outfh);
	unsigned char buf[16];
	while(fread(buf, blocksize, 1, infh) > 0)
	{
		if(mode == DXT1)
			convert_dxt1a(buf);
		else
			convert_dxt1(buf + 8);
		if(mode == DXT5)
			convert_dxt5(buf);
		fwrite(buf, blocksize, 1, outfh);
	}

	if(infile)
		fclose(infh);
	if(outfile)
		fclose(outfh);

	return 0;
}
