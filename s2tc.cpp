#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>

template <class T> inline T min(const T &a, const T &b)
{
	if(b < a)
		return b;
	return a;
}

template <class T> inline T max(const T &a, const T &b)
{
	if(b > a)
		return b;
	return a;
}

/* START stuff that originates from image.c in DarkPlaces */
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
/* end of darkplaces stuff */




typedef struct
{
	signed char r, g, b;
}
color_t;
inline bool operator<(const color_t &a, const color_t &b)
{
	signed char d;
	d = a.r - b.r;
	if(d)
		return d < 0;
	d = a.g - b.g;
	if(d)
		return d < 0;
	d = a.b - b.b;
	return d < 0;
}

// 16 differences must fit in int
// i.e. a difference must be lower than 2^27

// shift right, rounded
#define SHRR(a,n) (((a) + (1 << ((n)-1))) >> (n))

inline int color_dist_avg(const color_t &a, const color_t &b)
{
	int dr = a.r - b.r; // multiplier: 31 (-1..1)
	int dg = a.g - b.g; // multiplier: 63 (-1..1)
	int db = a.b - b.b; // multiplier: 31 (-1..1)
	return ((dr*dr) << 2) + dg*dg + ((db*db) << 2);
}

inline int color_dist_yuv(const color_t &a, const color_t &b)
{
	int dr = a.r - b.r; // multiplier: 31 (-1..1)
	int dg = a.g - b.g; // multiplier: 63 (-1..1)
	int db = a.b - b.b; // multiplier: 31 (-1..1)
	int y = dr * 30*2 + dg * 59 + db * 11*2; // multiplier: 6259
	int u = dr * 202 - y; // * 0.5 / (1 - 0.30)
	int v = db * 202 - y; // * 0.5 / (1 - 0.11)
	return ((y*y) << 1) + SHRR(u*u, 3) + SHRR(v*v, 4);
	// weight for u: sqrt(2^-4) / (0.5 / (1 - 0.30)) = 0.350
	// weight for v: sqrt(2^-5) / (0.5 / (1 - 0.11)) = 0.315
}

inline int color_dist_rgb(const color_t &a, const color_t &b)
{
	int dr = a.r - b.r; // multiplier: 31 (-1..1)
	int dg = a.g - b.g; // multiplier: 63 (-1..1)
	int db = a.b - b.b; // multiplier: 31 (-1..1)
	int y = dr * 21*2 + dg * 72 + db * 7*2; // multiplier: 6272
	int u = dr * 202 - y; // * 0.5 / (1 - 0.21)
	int v = db * 202 - y; // * 0.5 / (1 - 0.07)
	return ((y*y) << 1) + SHRR(u*u, 3) + SHRR(v*v, 4);
	// weight for u: sqrt(2^-4) / (0.5 / (1 - 0.21)) = 0.395
	// weight for v: sqrt(2^-5) / (0.5 / (1 - 0.07)) = 0.328
}

inline int color_dist_srgb(const color_t &a, const color_t &b)
{
	int dr = a.r * (int) a.r - b.r * (int) b.r; // multiplier: 31*31
	int dg = a.g * (int) a.g - b.g * (int) b.g; // multiplier: 63*63
	int db = a.b * (int) a.b - b.b * (int) b.b; // multiplier: 31*31
	int y = dr * 21*2*2 + dg * 72 + db * 7*2*2; // multiplier: 393400
	int u = dr * 409 - y; // * 0.5 / (1 - 0.30)
	int v = db * 409 - y; // * 0.5 / (1 - 0.11)
	int sy = SHRR(y, 3) * SHRR(y, 4);
	int su = SHRR(u, 3) * SHRR(u, 4);
	int sv = SHRR(v, 3) * SHRR(v, 4);
	return SHRR(sy, 4) + SHRR(su, 8) + SHRR(sv, 9);
	// weight for u: sqrt(2^-4) / (0.5 / (1 - 0.30)) = 0.350
	// weight for v: sqrt(2^-5) / (0.5 / (1 - 0.11)) = 0.315
}

inline int srgb_get_y(const color_t &a)
{
	// convert to linear
	int r = a.r * (int) a.r;
	int g = a.g * (int) a.g;
	int b = a.b * (int) a.b;
	// find luminance
	int y = 37 * (r * 21*2*2 + g * 72 + b * 7*2*2); // multiplier: 14555800
	// square root it (!)
	y = sqrt(y); // now in range 0 to 3815
	return y;
}

inline int color_dist_srgb_mixed(const color_t &a, const color_t &b)
{
	// get Y
	int ay = srgb_get_y(a);
	int by = srgb_get_y(b);
	// get UV
	int au = a.r * 191 - ay;
	int av = a.b * 191 - ay;
	int bu = b.r * 191 - by;
	int bv = b.b * 191 - by;
	// get differences
	int y = ay - by;
	int u = au - bu;
	int v = av - bv;
	return ((y*y) << 3) + SHRR(u*u, 1) + SHRR(v*v, 2);
	// weight for u: ???
	// weight for v: ???
}

// FIXME this is likely broken
inline int color_dist_lab_srgb(const color_t &a, const color_t &b)
{
	// undo sRGB
	float ar = powf(a.r / 31.0f, 2.4f);
	float ag = powf(a.g / 63.0f, 2.4f);
	float ab = powf(a.b / 31.0f, 2.4f);
	float br = powf(b.r / 31.0f, 2.4f);
	float bg = powf(b.g / 63.0f, 2.4f);
	float bb = powf(b.b / 31.0f, 2.4f);
	// convert to CIE XYZ
	float aX = 0.4124f * ar + 0.3576f * ag + 0.1805f * ab;
	float aY = 0.2126f * ar + 0.7152f * ag + 0.0722f * ab;
	float aZ = 0.0193f * ar + 0.1192f * ag + 0.9505f * ab;
	float bX = 0.4124f * br + 0.3576f * bg + 0.1805f * bb;
	float bY = 0.2126f * br + 0.7152f * bg + 0.0722f * bb;
	float bZ = 0.0193f * br + 0.1192f * bg + 0.9505f * bb;
	// convert to CIE Lab
	float Xn = 0.3127f;
	float Yn = 0.3290f;
	float Zn = 0.3583f;
	float aL = 116 * cbrtf(aY / Yn) - 16;
	float aA = 500 * (cbrtf(aX / Xn) - cbrtf(aY / Yn));
	float aB = 200 * (cbrtf(aY / Yn) - cbrtf(aZ / Zn));
	float bL = 116 * cbrtf(bY / Yn) - 16;
	float bA = 500 * (cbrtf(bX / Xn) - cbrtf(bY / Yn));
	float bB = 200 * (cbrtf(bY / Yn) - cbrtf(bZ / Zn));
	// euclidean distance, but moving weight away from A and B
	return 1000 * ((aL - bL) * (aL - bL) + (aA - bA) * (aA - bA) + (aB - bB) * (aB - bB));
}

inline int color_dist_normalmap(const color_t &a, const color_t &b)
{
	float ca[3], cb[3];
	ca[0] = a.r / 31.0 * 2 - 1;
	ca[1] = a.g / 63.0 * 2 - 1;
	ca[2] = a.b / 31.0 * 2 - 1;
	cb[0] = b.r / 31.0 * 2 - 1;
	cb[1] = b.g / 63.0 * 2 - 1;
	cb[2] = b.b / 31.0 * 2 - 1;

	return
		500 *
		(
			(cb[0] - ca[0]) * (cb[0] - ca[0])
			+
			(cb[1] - ca[1]) * (cb[1] - ca[1])
			+
			(cb[2] - ca[2]) * (cb[2] - ca[2])
		)
		;
	// max value: 500 * (4 + 4 + 4) = 6000
}

typedef int ColorDistFunc(const color_t &a, const color_t &b);

inline int alpha_dist(unsigned char a, unsigned char b)
{
	return (a - (int) b) * (a - (int) b);
}

template <class T, class F>
// n: input count
// m: total color count (including non-counted inputs)
// m >= n
void reduce_colors_inplace(T *c, int n, int m, F dist)
{
	int i, j, k;
	int bestsum = -1;
	int besti = 0;
	int bestj = 1;
	int dists[m][n];
	// first the square
	for(i = 0; i < n; ++i)
	{
		dists[i][i] = 0;
		for(j = i+1; j < n; ++j)
		{
			int d = dist(c[i], c[j]);
			dists[i][j] = dists[j][i] = d;
		}
	}
	// then the box
	for(; i < m; ++i)
	{
		for(j = 0; j < n; ++j)
		{
			int d = dist(c[i], c[j]);
			dists[i][j] = d;
		}
	}
	for(i = 0; i < m; ++i)
		for(j = i+1; j < m; ++j)
		{
			int sum = 0;
			for(k = 0; k < n; ++k)
			{
				int di = dists[i][k];
				int dj = dists[j][k];
				int m  = min(di, dj);
				sum += m;
			}
			if(bestsum < 0 || sum < bestsum)
			{
				bestsum = sum;
				besti = i;
				bestj = j;
			}
		}
	if(besti != 0)
		c[0] = c[besti];
	if(bestj != 1)
		c[1] = c[bestj];
}
template <class T, class F>
void reduce_colors_inplace_2fixpoints(T *c, int n, int m, F dist, const T &fix0, const T &fix1)
{
	int i, j, k;
	int bestsum = -1;
	int besti = 0;
	int bestj = 1;
	int dists[m+2][n];
	// first the square
	for(i = 0; i < n; ++i)
	{
		dists[i][i] = 0;
		for(j = i+1; j < n; ++j)
		{
			int d = dist(c[i], c[j]);
			dists[i][j] = dists[j][i] = d;
		}
	}
	// then the box
	for(; i < m; ++i)
	{
		for(j = 0; j < n; ++j)
		{
			int d = dist(c[i], c[j]);
			dists[i][j] = d;
		}
	}
	// then the two extra rows
	for(j = 0; j < n; ++j)
	{
		int d = dist(fix0, c[j]);
		dists[m][j] = d;
	}
	for(j = 0; j < n; ++j)
	{
		int d = dist(fix1, c[j]);
		dists[m+1][j] = d;
	}
	for(i = 0; i < m; ++i)
		for(j = i+1; j < m; ++j)
		{
			int sum = 0;
			for(k = 0; k < n; ++k)
			{
				int di = dists[i][k];
				int dj = dists[j][k];
				int d0 = dists[m][k];
				int d1 = dists[m+1][k];
				int m  = min(min(di, dj), min(d0, d1));
				sum += m;
			}
			if(bestsum < 0 || sum < bestsum)
			{
				bestsum = sum;
				besti = i;
				bestj = j;
			}
		}
	if(besti != 0)
		c[0] = c[besti];
	if(bestj != 1)
		c[1] = c[bestj];
}

inline int diffuse(float *diff, float src)
{
	int ret;
	src += *diff;
	ret = src;
	*diff = (src - ret);
	return ret;
}

void rgb565_image(unsigned char *out, const unsigned char *rgba, int w, int h, int alpharange)
{
	int x, y;
	float diffuse_r = 0;
	float diffuse_g = 0;
	float diffuse_b = 0;
	float diffuse_a = 0;
	for(y = 0; y < h; ++y)
		for(x = 0; x < w; ++x)
		{
			out[(x + y * w) * 4 + 2] = diffuse(&diffuse_r, rgba[(x + y * w) * 4 + 2] * 31.0 / 255.0);
			out[(x + y * w) * 4 + 1] = diffuse(&diffuse_g, rgba[(x + y * w) * 4 + 1] * 63.0 / 255.0);
			out[(x + y * w) * 4 + 0] = diffuse(&diffuse_b, rgba[(x + y * w) * 4 + 0] * 31.0 / 255.0);
			out[(x + y * w) * 4 + 3] = diffuse(&diffuse_a, rgba[(x + y * w) * 4 + 3] * (alpharange / 255.0));
		}
}

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

template<DxtMode dxt, ColorDistFunc ColorDist, bool userandom>
void s2tc_encode_block(unsigned char *out, const unsigned char *rgba, int iw, int w, int h, int nrandom)
{
	color_t c[16 + (userandom ? nrandom : 0)];

	unsigned char ca[16];
	int n = 0, m = 0;
	int x, y;

	for(x = 0; x < w; ++x)
		for(y = 0; y < h; ++y)
		{
			c[n].r = rgba[(x + y * iw) * 4 + 2];
			c[n].g = rgba[(x + y * iw) * 4 + 1];
			c[n].b = rgba[(x + y * iw) * 4 + 0];
			if(dxt == DXT5)
				ca[n]  = rgba[(x + y * iw) * 4 + 3];
			++n;
		}

	m = n;

	if(userandom)
	{
		color_t mins = c[0];
		color_t maxs = c[0];
		for(x = 1; x < n; ++x)
		{
			mins.r = min(mins.r, c[x].r);
			mins.g = min(mins.g, c[x].g);
			mins.b = min(mins.b, c[x].b);
			maxs.r = max(maxs.r, c[x].r);
			maxs.g = max(maxs.g, c[x].g);
			maxs.b = max(maxs.b, c[x].b);
		}
		color_t len = { maxs.r - mins.r + 1, maxs.g - mins.g + 1, maxs.b - mins.b + 1 };
		for(x = 0; x < nrandom; ++x)
		{
			c[m].r = mins.r + rand() % len.r;
			c[m].g = mins.g + rand() % len.g;
			c[m].b = mins.b + rand() % len.b;
			++m;
		}
	}

	reduce_colors_inplace(c, n, m, ColorDist);
	if(dxt == DXT5)
	{
		reduce_colors_inplace_2fixpoints(ca, n, n, alpha_dist, (unsigned char) 0, (unsigned char) 255);
		if(ca[1] < ca[0])
		{
			ca[2] = ca[0];
			ca[0] = ca[1];
			ca[1] = ca[2];
		}
	}
	if(c[1] < c[0])
	{
		c[2] = c[0];
		c[0] = c[1];
		c[1] = c[2];
	}

	memset(out, 0, 16);
	switch(dxt)
	{
		case DXT5:
			out[0] = ca[0];
			out[1] = ca[1];
		case DXT3:
			out[8] = ((c[0].g & 0x07) << 5) | c[0].b;
			out[9] = (c[0].r << 3) | (c[0].g >> 3);
			out[10] = ((c[1].g & 0x07) << 5) | c[1].b;
			out[11] = (c[1].r << 3) | (c[1].g >> 3);
			break;
		case DXT1:
			out[0] = ((c[0].g & 0x07) << 5) | c[0].b;
			out[1] = (c[0].r << 3) | (c[0].g >> 3);
			out[2] = ((c[1].g & 0x07) << 5) | c[1].b;
			out[3] = (c[1].r << 3) | (c[1].g >> 3);
			break;
	}
	for(x = 0; x < w; ++x)
		for(y = 0; y < h; ++y)
		{
			int pindex = (x+y*4);
			c[2].r = rgba[(x + y * iw) * 4 + 2];
			c[2].g = rgba[(x + y * iw) * 4 + 1];
			c[2].b = rgba[(x + y * iw) * 4 + 0];
			ca[2]  = rgba[(x + y * iw) * 4 + 3];
			switch(dxt)
			{
				case DXT5:
					{
						int da[4];
						int bitindex = pindex * 3;
						da[0] = alpha_dist(ca[0], ca[2]);
						da[1] = alpha_dist(ca[1], ca[2]);
						da[2] = alpha_dist(0, ca[2]);
						da[3] = alpha_dist(255, ca[2]);
						if(da[2] <= da[0] && da[2] <= da[1] && da[2] <= da[3])
						{
							// 6
							++bitindex;
							out[bitindex / 8 + 2] |= (1 << (bitindex % 8));
							++bitindex;
							out[bitindex / 8 + 2] |= (1 << (bitindex % 8));
						}
						else if(da[3] <= da[0] && da[3] <= da[1])
						{
							// 7
							out[bitindex / 8 + 2] |= (1 << (bitindex % 8));
							++bitindex;
							out[bitindex / 8 + 2] |= (1 << (bitindex % 8));
							++bitindex;
							out[bitindex / 8 + 2] |= (1 << (bitindex % 8));
						}
						else if(da[0] <= da[1])
						{
							// 0
						}
						else
						{
							// 1
							out[bitindex / 8 + 2] |= (1 << (bitindex % 8));
						}
					}
					if(ColorDist(c[0], c[2]) > ColorDist(c[1], c[2]))
					{
						int bitindex = pindex * 2;
						out[bitindex / 8 + 12] |= (1 << (bitindex % 8));
					}
					break;
				case DXT3:
					{
						int bitindex = pindex * 4;
						out[bitindex / 8 + 0] |= (ca[2] << (bitindex % 8));
					}
					if(ColorDist(c[0], c[2]) > ColorDist(c[1], c[2]))
					{
						int bitindex = pindex * 2;
						out[bitindex / 8 + 12] |= (1 << (bitindex % 8));
					}
					break;
				case DXT1:
					{
						int bitindex = pindex * 2;
						if(!ca[2])
							out[bitindex / 8 + 4] |= (3 << (bitindex % 8));
						else if(ColorDist(c[0], c[2]) > ColorDist(c[1], c[2]))
							out[bitindex / 8 + 4] |= (1 << (bitindex % 8));
					}
					break;
			}
		}
}

// compile time dispatch magic
template<DxtMode dxt, ColorDistFunc ColorDist>
void s2tc_encode_block(unsigned char *out, const unsigned char *rgba, int iw, int w, int h, int nrandom)
{
	if(nrandom)
		s2tc_encode_block<dxt, ColorDist, true>(out, rgba, iw, w, h, nrandom);
	else
		s2tc_encode_block<dxt, ColorDist, false>(out, rgba, iw, w, h, nrandom);
}

template<ColorDistFunc ColorDist>
void s2tc_encode_block(unsigned char *out, const unsigned char *rgba, int iw, int w, int h, DxtMode dxt, int nrandom)
{
	switch(dxt)
	{
		case DXT1:
			s2tc_encode_block<DXT1, ColorDist>(out, rgba, iw, w, h, nrandom);
			break;
		case DXT3:
			s2tc_encode_block<DXT3, ColorDist>(out, rgba, iw, w, h, nrandom);
			break;
		default:
		case DXT5:
			s2tc_encode_block<DXT5, ColorDist>(out, rgba, iw, w, h, nrandom);
			break;
	}
}

void s2tc_encode_block(unsigned char *out, const unsigned char *rgba, int iw, int w, int h, DxtMode dxt, ColorDistMode cd, int nrandom)
{
	switch(cd)
	{
		case RGB:
			s2tc_encode_block<color_dist_rgb>(out, rgba, iw, w, h, dxt, nrandom);
			break;
		case YUV:
			s2tc_encode_block<color_dist_yuv>(out, rgba, iw, w, h, dxt, nrandom);
			break;
		case SRGB:
			s2tc_encode_block<color_dist_srgb>(out, rgba, iw, w, h, dxt, nrandom);
			break;
		case SRGB_MIXED:
			s2tc_encode_block<color_dist_srgb_mixed>(out, rgba, iw, w, h, dxt, nrandom);
			break;
		case LAB:
			s2tc_encode_block<color_dist_lab_srgb>(out, rgba, iw, w, h, dxt, nrandom);
			break;
		case AVG:
			s2tc_encode_block<color_dist_avg>(out, rgba, iw, w, h, dxt, nrandom);
			break;
		case NORMALMAP:
			s2tc_encode_block<color_dist_normalmap>(out, rgba, iw, w, h, dxt, nrandom);
			break;
	}
}

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
			"    [-r randomcount]\n"
			"    [-c {RGB|YUV|SRGB|SRGB_MIXED|LAB|AVG|NORMALMAP}]\n",
			me);
	return 1;
}

int main(int argc, char **argv)
{
	int x, y;
	unsigned char *pic, *picdata;
	int piclen;
	const char *fourcc;
	int blocksize, alpharange;
	DxtMode dxt = DXT1;
	ColorDistMode cd = RGB;
	int nrandom = 0;
	const char *infile = NULL, *outfile = NULL;
	FILE *outfh;

	int opt;
	while((opt = getopt(argc, argv, "i:o:t:r:c:")) != -1)
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
					dxt = DXT1;
				else if(!strcasecmp(optarg, "DXT3"))
					dxt = DXT3;
				else if(!strcasecmp(optarg, "DXT5"))
					dxt = DXT5;
				else
					return usage(argv[0]);
				break;
			case 'r':
				nrandom = atoi(optarg);
				break;
			case 'c':
				if(!strcasecmp(optarg, "RGB"))
					cd = RGB;
				else if(!strcasecmp(optarg, "YUV"))
					cd = YUV;
				else if(!strcasecmp(optarg, "SRGB"))
					cd = SRGB;
				else if(!strcasecmp(optarg, "SRGB_MIXED"))
					cd = SRGB_MIXED;
				else if(!strcasecmp(optarg, "LAB"))
					cd = LAB;
				else if(!strcasecmp(optarg, "AVG"))
					cd = AVG;
				else if(!strcasecmp(optarg, "NORMALMAP"))
					cd = NORMALMAP;
				else
					return usage(argv[0]);
				break;
			default:
				return usage(argv[0]);
				break;
		}
	}

	outfh = outfile ? fopen(outfile, "wb") : stdout;
	if(!outfh)
	{
		printf("opening output failed\n");
		return 2;
	}

	picdata = FS_LoadFile(infile, &piclen);
	if(!picdata)
	{
		printf("FS_LoadFile failed\n");
		return 2;
	}
	pic = LoadTGA_BGRA(picdata, piclen);

	int mipcount = 0;
	while(image_width >= (1 << mipcount) || image_height >= (1 << mipcount))
		++mipcount;
	// now, (1 << mipcount) >= width, height

	switch(dxt)
	{
		case DXT1:
			blocksize = 8;
			alpharange = 1;
			fourcc = "DXT1";
			break;
		case DXT3:
			blocksize = 16;
			alpharange = 15;
			fourcc = "DXT3";
			break;
		default:
		case DXT5:
			blocksize = 16;
			alpharange = 255;
			fourcc = "DXT5";
			break;
	}

	{
		uint32_t picsize = LittleLong(((image_width+3)/4) * ((image_height+3)/4) * blocksize);
		uint32_t ddssize = LittleLong(0x7c);
		uint32_t dds_flags = LittleLong(0xa1007);
		uint32_t one = LittleLong(1);
		uint32_t zero = LittleLong(0);
		uint32_t dds_format_flags = LittleLong(0x04);
		uint32_t dds_caps1 = LittleLong(0x401008);
		uint32_t dds_caps2 = LittleLong(0);
		uint32_t dds_format_size = LittleLong(32);
		uint32_t dds_mipcount = LittleLong(mipcount);

		//0
		fwrite("DDS ", 4, 1, outfh);
		fwrite(&ddssize, 4, 1, outfh);
		fwrite(&dds_flags, 4, 1, outfh);
		fwrite(&image_height, 4, 1, outfh);
		fwrite(&image_width, 4, 1, outfh);
		fwrite(&picsize, 4, 1, outfh);
		fwrite(&one, 4, 1, outfh);
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
		fwrite(&dds_format_size, 4, 1, outfh);
		fwrite(&dds_format_flags, 4, 1, outfh);
		fwrite(fourcc, 4, 1, outfh);
		fwrite("\x18\x00\x00\x00", 4, 1, outfh);
		fwrite("\x00\x00\xff\x00", 4, 1, outfh);

		//96
		fwrite("\x00\xff\x00\x00", 4, 1, outfh);
		fwrite("\xff\x00\x00\x00", 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&dds_caps1, 4, 1, outfh);
		fwrite(&dds_caps2, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
		fwrite(&zero, 4, 1, outfh);
	}

	unsigned char *opic = (unsigned char *) malloc(image_width * image_height * 4);
	for(;;)
	{
		rgb565_image(opic, pic, image_width, image_height, alpharange);
		for(y = 0; y < image_height; y += 4)
			for(x = 0; x < image_width; x += 4)
			{
				unsigned char block[16];
				s2tc_encode_block(block, opic + (x + y * image_width) * 4, image_width, min(4, image_width - x), min(4, image_height - y), dxt, cd, nrandom);
				fwrite(block, blocksize, 1, outfh);
			}
		if(image_width == 1 && image_height == 1)
			break;
		Image_MipReduce32(pic, pic, &image_width, &image_height, 1, 1);
	}

	if(outfile)
		fclose(outfh);

	return 0;
}
