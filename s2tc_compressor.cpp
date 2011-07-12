#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "s2tc_compressor.h"
#include "s2tc_common.h"

namespace
{
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
};

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

namespace
{
	inline int diffuse(float *diff, float src)
	{
		int ret;
		src += *diff;
		ret = src;
		*diff = (src - ret);
		return ret;
	}
};

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

