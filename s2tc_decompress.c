#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "libtxc_dxtn.h"

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

int main()
{
	uint32_t h[32];
	fread(h, sizeof(h), 1, stdin);
	int height = LittleLong(h[3]);
	int width = LittleLong(h[4]);
	int fourcc = LittleLong(h[21]);
	void (*fetch)(GLint srcRowStride, const GLubyte *pixdata, GLint i, GLint j, GLvoid *texel) = NULL;
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
	fwrite(t, 18, 1, stdout);

	int n = ((width + 3) / 4) * ((height + 3) / 4);
	unsigned char *buf = malloc(n * blocksize);
	fread(buf, blocksize, n, stdin);

	int x, y;
	for(y = 0; y < height; ++y)
		for(x = 0; x < width; ++x)
		{
			char data[4];
			fetch(width, buf, x, y, &data);
			fwrite(data, 4, 1, stdout);
		}
	return 0;
}
