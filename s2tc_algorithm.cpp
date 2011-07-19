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
#define S2TC_LICENSE_IDENTIFIER s2tc_algorithm_license
#include "s2tc_license.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "s2tc_algorithm.h"
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

	inline int color_dist_wavg(const color_t &a, const color_t &b)
	{
		int dr = a.r - b.r; // multiplier: 31 (-1..1)
		int dg = a.g - b.g; // multiplier: 63 (-1..1)
		int db = a.b - b.b; // multiplier: 31 (-1..1)
		return ((dr*dr) << 2) + ((dg*dg) << 2) + (db*db);
		// weighted 4:16:1
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
		y = sqrtf(y) + 0.5f; // now in range 0 to 3815
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

	inline int color_dist_normalmap(const color_t &a, const color_t &b)
	{
		float ca[3], cb[3], n;
		ca[0] = a.r / 31.0f * 2 - 1;
		ca[1] = a.g / 63.0f * 2 - 1;
		ca[2] = a.b / 31.0f * 2 - 1;
		cb[0] = b.r / 31.0f * 2 - 1;
		cb[1] = b.g / 63.0f * 2 - 1;
		cb[2] = b.b / 31.0f * 2 - 1;
		n = ca[0] * ca[0] + ca[1] * ca[1] + ca[2] * ca[2];
		if(n > 0)
		{
			n = 1.0f / sqrtf(n);
			ca[0] *= n;
			ca[1] *= n;
			ca[2] *= n;
		}
		n = cb[0] * cb[0] + cb[1] * cb[1] + cb[2] * cb[2];
		if(n > 0)
		{
			n = 1.0f / sqrtf(n);
			cb[0] *= n;
			cb[1] *= n;
			cb[2] *= n;
		}

		return
			100000 *
			(
				(cb[0] - ca[0]) * (cb[0] - ca[0])
				+
				(cb[1] - ca[1]) * (cb[1] - ca[1])
				+
				(cb[2] - ca[2]) * (cb[2] - ca[2])
			)
			;
		// max value: 1000 * (4 + 4 + 4) = 6000
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
	inline void reduce_colors_inplace(T *c, int n, int m, F dist)
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
	inline void reduce_colors_inplace_2fixpoints(T *c, int n, int m, F dist, const T &fix0, const T &fix1)
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

	enum CompressionMode
	{
		MODE_NORMAL,
		MODE_RANDOM,
		MODE_FAST
	};

	template<ColorDistFunc ColorDist> inline int refine_component_encode(int comp)
	{
		return comp;
	}
	template<> inline int refine_component_encode<color_dist_srgb>(int comp)
	{
		return comp * comp;
	}
	template<> inline int refine_component_encode<color_dist_srgb_mixed>(int comp)
	{
		return comp * comp;
	}

	template<ColorDistFunc ColorDist> inline int refine_component_decode(int comp)
	{
		return comp;
	}
	template<> inline int refine_component_decode<color_dist_srgb>(int comp)
	{
		return sqrtf(comp) + 0.5f;
	}
	template<> inline int refine_component_decode<color_dist_srgb_mixed>(int comp)
	{
		return sqrtf(comp) + 0.5f;
	}

	// these color dist functions ignore color values at alpha 0
	template<ColorDistFunc ColorDist> struct alpha_0_is_unimportant
	{
		static bool const value = true;
	};
	template<> struct alpha_0_is_unimportant<color_dist_normalmap>
	{
		static bool const value = false;
	};

	template<DxtMode dxt, ColorDistFunc ColorDist, CompressionMode mode, RefinementMode refine>
	inline void s2tc_encode_block(unsigned char *out, const unsigned char *rgba, int iw, int w, int h, int nrandom)
	{
		color_t c[16 + (mode == MODE_RANDOM ? nrandom : 0)];
		unsigned char ca[16 + (mode == MODE_RANDOM ? nrandom : 0)];
		int n = 0, m = 0;
		int x, y;

		if(mode == MODE_FAST)
		{
			// FAST: trick from libtxc_dxtn: just get brightest and darkest colors, and encode using these

			color_t c0 = {0, 0, 0};

			// dummy values because we don't know whether the first pixel willw rite
			c[0].r = 31;
			c[0].g = 63;
			c[0].b = 31;
			c[1].r = 0;
			c[1].g = 0;
			c[1].b = 0;
			int dmin = 0x7FFFFFFF;
			int dmax = 0;
			if(dxt == DXT5)
			{
				ca[0] = rgba[3];
				ca[1] = ca[0];
			}

			for(x = 0; x < w; ++x)
				for(y = 0; y < h; ++y)
				{
					c[2].r = rgba[(x + y * iw) * 4 + 2];
					c[2].g = rgba[(x + y * iw) * 4 + 1];
					c[2].b = rgba[(x + y * iw) * 4 + 0];
					ca[2]  = rgba[(x + y * iw) * 4 + 3];
					// MODE_FAST doesn't work for normalmaps, so this works
					if(!ca[2])
						continue;

					int d = ColorDist(c[2], c0);
					if(d > dmax)
					{
						dmax = d;
						c[1] = c[2];
					}
					if(d < dmin)
					{
						dmin = d;
						c[0] = c[2];
					}

					if(dxt == DXT5)
					{
						if(ca[2] != 255)
						{
							if(ca[2] > ca[1])
								ca[1] = ca[2];
							if(ca[2] < ca[0])
								ca[0] = ca[2];
						}
					}
				}

			// if ALL pixels were transparent, this won't stop us

			m = n = 2;
		}
		else
		{
			for(x = 0; x < w; ++x)
				for(y = 0; y < h; ++y)
				{
					ca[n]  = rgba[(x + y * iw) * 4 + 3];
					if(alpha_0_is_unimportant<ColorDist>::value)
						if(!ca[n])
							continue;
					c[n].r = rgba[(x + y * iw) * 4 + 2];
					c[n].g = rgba[(x + y * iw) * 4 + 1];
					c[n].b = rgba[(x + y * iw) * 4 + 0];
					++n;
				}
			if(n == 0)
			{
				n = 1;
				c[0].r = 0;
				c[0].g = 0;
				c[0].b = 0;
				ca[0] = 0;
			}
			m = n;

			if(mode == MODE_RANDOM)
			{
				color_t mins = c[0];
				color_t maxs = c[0];
				unsigned char mina = (dxt == DXT5) ? ca[0] : 0;
				unsigned char maxa = (dxt == DXT5) ? ca[0] : 0;
				for(x = 1; x < n; ++x)
				{
					mins.r = min(mins.r, c[x].r);
					mins.g = min(mins.g, c[x].g);
					mins.b = min(mins.b, c[x].b);
					maxs.r = max(maxs.r, c[x].r);
					maxs.g = max(maxs.g, c[x].g);
					maxs.b = max(maxs.b, c[x].b);
					if(dxt == DXT5)
					{
						mina = min(mina, ca[x]);
						maxa = max(maxa, ca[x]);
					}
				}
				color_t len = { maxs.r - mins.r + 1, maxs.g - mins.g + 1, maxs.b - mins.b + 1 };
				int lena = (dxt == DXT5) ? (maxa - (int) mina + 1) : 0;
				for(x = 0; x < nrandom; ++x)
				{
					c[m].r = mins.r + rand() % len.r;
					c[m].g = mins.g + rand() % len.g;
					c[m].b = mins.b + rand() % len.b;
					if(dxt == DXT5)
						ca[m] = mina + rand() % lena;
					++m;
				}
			}
			else
			{
				// hack for last miplevel
				if(n == 1)
				{
					c[1] = c[0];
					m = n = 2;
				}
			}

			reduce_colors_inplace(c, n, m, ColorDist);
			if(dxt == DXT5)
				reduce_colors_inplace_2fixpoints(ca, n, m, alpha_dist, (unsigned char) 0, (unsigned char) 255);
		}

		if(refine == REFINE_NEVER)
		{
			if(dxt == DXT5)
			{
				if(ca[1] < ca[0])
				{
					// select mode with 6 = 0, 7 = 255
					ca[2] = ca[0];
					ca[0] = ca[1];
					ca[1] = ca[2];
				}
			}
			if((dxt == DXT1) ? (c[1] < c[0]) : (c[0] < c[1]))
			// DXT1: select mode with 3 = transparent
			// other: don't select this mode
			{
				c[2] = c[0];
				c[0] = c[1];
				c[1] = c[2];
			}
		}

		bool refined;
		do
		{
			int nc0 = 0, na0 = 0, sc0r = 0, sc0g = 0, sc0b = 0, sa0 = 0;
			int nc1 = 0, na1 = 0, sc1r = 0, sc1g = 0, sc1b = 0, sa1 = 0;
			if(refine == REFINE_LOOP)
				refined = false;

			memset(out, 0, (dxt == DXT1) ? 8 : 16);
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
								bool visible = true;
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
									setbit(&out[2], bitindex);
									++bitindex;
									setbit(&out[2], bitindex);
									if(alpha_0_is_unimportant<ColorDist>::value)
										visible = false;
								}
								else if(da[3] <= da[0] && da[3] <= da[1])
								{
									// 7
									setbit(&out[2], bitindex);
									++bitindex;
									setbit(&out[2], bitindex);
									++bitindex;
									setbit(&out[2], bitindex);
								}
								else if(da[0] <= da[1])
								{
									// 0
									if(refine != REFINE_NEVER)
									{
										++na0;
										sa0 += ca[2];
									}
								}
								else
								{
									// 1
									setbit(&out[2], bitindex);
									if(refine != REFINE_NEVER)
									{
										++na1;
										sa1 += ca[2];
									}
								}
								if(ColorDist(c[0], c[2]) > ColorDist(c[1], c[2]))
								{
									int bitindex = pindex * 2;
									setbit(&out[12], bitindex);
									if(refine != REFINE_NEVER)
									{
										if(!alpha_0_is_unimportant<ColorDist>::value || visible)
										{
											++nc1;
											sc1r += refine_component_encode<ColorDist>(c[2].r);
											sc1g += refine_component_encode<ColorDist>(c[2].g);
											sc1b += refine_component_encode<ColorDist>(c[2].b);
										}
									}
								}
								else
								{
									if(refine != REFINE_NEVER)
									{
										if(!alpha_0_is_unimportant<ColorDist>::value || visible)
										{
											++nc0;
											sc0r += refine_component_encode<ColorDist>(c[2].r);
											sc0g += refine_component_encode<ColorDist>(c[2].g);
											sc0b += refine_component_encode<ColorDist>(c[2].b);
										}
									}
								}
							}
							break;
						case DXT3:
							{
								int bitindex = pindex * 4;
								setbit(&out[0], bitindex, ca[2]);
							}
							if(ColorDist(c[0], c[2]) > ColorDist(c[1], c[2]))
							{
								int bitindex = pindex * 2;
								setbit(&out[12], bitindex);
								if(refine != REFINE_NEVER)
								{
									if(!alpha_0_is_unimportant<ColorDist>::value || ca[2])
									{
										++nc1;
										sc1r += refine_component_encode<ColorDist>(c[2].r);
										sc1g += refine_component_encode<ColorDist>(c[2].g);
										sc1b += refine_component_encode<ColorDist>(c[2].b);
									}
								}
							}
							else
							{
								if(refine != REFINE_NEVER)
								{
									if(!alpha_0_is_unimportant<ColorDist>::value || ca[2])
									{
										++nc0;
										sc0r += refine_component_encode<ColorDist>(c[2].r);
										sc0g += refine_component_encode<ColorDist>(c[2].g);
										sc0b += refine_component_encode<ColorDist>(c[2].b);
									}
								}
							}
							break;
						case DXT1:
							{
								// the normalmap-uses-alpha-0 hack cannot be used here
								int bitindex = pindex * 2;
								if(!ca[2])
									setbit(&out[4], bitindex, 3);
								else if(ColorDist(c[0], c[2]) > ColorDist(c[1], c[2]))
								{
									setbit(&out[4], bitindex);
									if(refine != REFINE_NEVER)
									{
										++nc1;
										sc1r += refine_component_encode<ColorDist>(c[2].r);
										sc1g += refine_component_encode<ColorDist>(c[2].g);
										sc1b += refine_component_encode<ColorDist>(c[2].b);
									}
								}
								else
								{
									if(refine != REFINE_NEVER)
									{
										++nc0;
										sc0r += refine_component_encode<ColorDist>(c[2].r);
										sc0g += refine_component_encode<ColorDist>(c[2].g);
										sc0b += refine_component_encode<ColorDist>(c[2].b);
									}
								}
							}
							break;
					}
				}
			if(refine != REFINE_NEVER)
			{
				// REFINEMENT: trick from libtxc_dxtn: reassign the colors to an average of the colors encoded with that value

				if(dxt == DXT5)
				{
					if(na0)
						ca[0] = (2 * sa0 + na0) / (2 * na0);
					if(na1)
						ca[1] = (2 * sa1 + na1) / (2 * na1);
				}
				if(refine == REFINE_CHECK || refine == REFINE_LOOP)
				{
					c[2] = c[0];
					c[3] = c[1];
				}
				if(nc0)
				{
					c[0].r = refine_component_decode<ColorDist>((2 * sc0r + nc0) / (2 * nc0));
					c[0].g = refine_component_decode<ColorDist>((2 * sc0g + nc0) / (2 * nc0));
					c[0].b = refine_component_decode<ColorDist>((2 * sc0b + nc0) / (2 * nc0));
				}
				if(nc1)
				{
					c[1].r = refine_component_decode<ColorDist>((2 * sc1r + nc1) / (2 * nc1));
					c[1].g = refine_component_decode<ColorDist>((2 * sc1g + nc1) / (2 * nc1));
					c[1].b = refine_component_decode<ColorDist>((2 * sc1b + nc1) / (2 * nc1));
				}

				if(refine == REFINE_CHECK || refine == REFINE_LOOP)
				{
					int score_01 = 0;
					int score_23 = 0;
					for(x = 0; x < w; ++x)
						for(y = 0; y < h; ++y)
						{
							int pindex = (x+y*4);
							c[4].r = rgba[(x + y * iw) * 4 + 2];
							c[4].g = rgba[(x + y * iw) * 4 + 1];
							c[4].b = rgba[(x + y * iw) * 4 + 0];
							if(alpha_0_is_unimportant<ColorDist>::value || dxt == DXT1) // in DXT1, alpha 0 pixels are always skipped!
							{
								if(dxt == DXT5)
								{
									// check ENCODED alpha
									int bitindex_0 = pindex * 3;
									int bitindex_1 = bitindex_0 + 2;
									if(!testbit(&out[2], bitindex_0))
										if(testbit(&out[2], bitindex_1))
											continue;
								}
								else
								{
									// check ORIGINAL alpha (DXT1 and DXT3 preserve it)
									ca[4] = rgba[(x + y * iw) * 4 + 3];
									if(!ca[4])
										continue;
								}
							}
							int bitindex = pindex * 2;
							if(refine == REFINE_CHECK)
							{
								if(testbit(&out[(dxt == DXT1 ? 4 : 12)], bitindex))
								{
									// we picked an 1
									score_01 += ColorDist(c[1], c[4]);
									score_23 += ColorDist(c[3], c[4]);
								}
								else
								{
									// we picked a 0
									score_01 += ColorDist(c[0], c[4]);
									score_23 += ColorDist(c[2], c[4]);
								}
							}
							else if(refine == REFINE_LOOP)
							{
								if(testbit(&out[(dxt == DXT1 ? 4 : 12)], bitindex))
								{
									// we picked an 1
									score_23 += ColorDist(c[3], c[4]);
								}
								else
								{
									// we picked a 0
									score_23 += ColorDist(c[2], c[4]);
								}
								// we WILL run another loop iteration, if score_01 wins
								score_01 += min(ColorDist(c[0], c[4]), ColorDist(c[1], c[4]));
							}
						}

					if(score_23 <= score_01)
					{
						// refinement was BAD
						c[0] = c[2];
						c[1] = c[3];
					}
					else if(refine == REFINE_LOOP)
						refined = true;

					// alpha refinement is always good and doesn't
					// need to be checked because alpha is linear

					// when looping, though, checking the
					// alpha COULD help, but we usually
					// loop twice anyway as refinement
					// usually helps
				}
			}
		}
		while(refine == REFINE_LOOP && refined);

		if(refine != REFINE_NEVER)
		{
			if(dxt == DXT5)
			{
				if(ca[1] < ca[0])
				{
					ca[2] = ca[0];
					ca[0] = ca[1];
					ca[1] = ca[2];
					// swap the alphas
					for(int pindex = 0; pindex < 16; ++pindex)
					{
						int bitindex_set = pindex * 3;
						int bitindex_test = bitindex_set + 2;
						if(!testbit(&out[2], bitindex_test))
							xorbit(&out[2], bitindex_set);
					}
				}
			}
			if((dxt == DXT1) ? (c[1] < c[0]) : (c[0] < c[1]))
			// DXT1: select mode with 3 = transparent
			// other: don't select this mode
			{
				c[2] = c[0];
				c[0] = c[1];
				c[1] = c[2];
				// swap the colors
				if(dxt == DXT1)
				{
					out[4] ^= 0x55 & ~(out[4] >> 1);
					out[5] ^= 0x55 & ~(out[5] >> 1);
					out[6] ^= 0x55 & ~(out[6] >> 1);
					out[7] ^= 0x55 & ~(out[7] >> 1);
				}
				else
				{
					out[12] ^= 0x55 & ~(out[12] >> 1);
					out[13] ^= 0x55 & ~(out[13] >> 1);
					out[14] ^= 0x55 & ~(out[14] >> 1);
					out[15] ^= 0x55 & ~(out[15] >> 1);
				}
			}
		}

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
	}

	// these color dist functions do not need the refinement check, as they always improve the situation
	template<ColorDistFunc ColorDist> struct need_refine_check
	{
		static const bool value = true;
	};
	template<> struct need_refine_check<color_dist_avg>
	{
		static const bool value = false;
	};
	template<> struct need_refine_check<color_dist_wavg>
	{
		static const bool value = false;
	};

	// compile time dispatch magic
	template<DxtMode dxt, ColorDistFunc ColorDist, CompressionMode mode>
	inline s2tc_encode_block_func_t s2tc_encode_block_func(RefinementMode refine)
	{
		switch(refine)
		{
			case REFINE_NEVER:
				return s2tc_encode_block<dxt, ColorDist, mode, REFINE_NEVER>;
			case REFINE_LOOP:
				return s2tc_encode_block<dxt, ColorDist, mode, REFINE_LOOP>;
			case REFINE_CHECK:
				if(need_refine_check<ColorDist>::value)
					return s2tc_encode_block<dxt, ColorDist, mode, REFINE_CHECK>;
			default:
			case REFINE_ALWAYS:
				return s2tc_encode_block<dxt, ColorDist, mode, REFINE_ALWAYS>;
		}
	}

	// these color dist functions do not need the refinement check, as they always improve the situation
	template<ColorDistFunc ColorDist> struct supports_fast
	{
		static const bool value = true;
	};
	template<> struct need_refine_check<color_dist_normalmap>
	{
		static const bool value = false;
	};

	template<DxtMode dxt, ColorDistFunc ColorDist>
	inline s2tc_encode_block_func_t s2tc_encode_block_func(int nrandom, RefinementMode refine)
	{
		if(nrandom > 0)
			return s2tc_encode_block_func<dxt, ColorDist, MODE_RANDOM>(refine);
		else if(!supports_fast<ColorDist>::value || nrandom == 0) // MODE_FAST not supported for normalmaps, sorry
			return s2tc_encode_block_func<dxt, ColorDist, MODE_NORMAL>(refine);
		else
			return s2tc_encode_block_func<dxt, ColorDist, MODE_FAST>(refine);
	}

	template<ColorDistFunc ColorDist>
	inline s2tc_encode_block_func_t s2tc_encode_block_func(DxtMode dxt, int nrandom, RefinementMode refine)
	{
		switch(dxt)
		{
			case DXT1:
				return s2tc_encode_block_func<DXT1, ColorDist>(nrandom, refine);
				break;
			case DXT3:
				return s2tc_encode_block_func<DXT3, ColorDist>(nrandom, refine);
				break;
			default:
			case DXT5:
				return s2tc_encode_block_func<DXT5, ColorDist>(nrandom, refine);
				break;
		}
	}
};

s2tc_encode_block_func_t s2tc_encode_block_func(DxtMode dxt, ColorDistMode cd, int nrandom, RefinementMode refine)
{
	switch(cd)
	{
		case RGB:
			return s2tc_encode_block_func<color_dist_rgb>(dxt, nrandom, refine);
			break;
		case YUV:
			return s2tc_encode_block_func<color_dist_yuv>(dxt, nrandom, refine);
			break;
		case SRGB:
			return s2tc_encode_block_func<color_dist_srgb>(dxt, nrandom, refine);
			break;
		case SRGB_MIXED:
			return s2tc_encode_block_func<color_dist_srgb_mixed>(dxt, nrandom, refine);
			break;
		case AVG:
			return s2tc_encode_block_func<color_dist_avg>(dxt, nrandom, refine);
			break;
		default:
		case WAVG:
			return s2tc_encode_block_func<color_dist_wavg>(dxt, nrandom, refine);
			break;
		case NORMALMAP:
			return s2tc_encode_block_func<color_dist_normalmap>(dxt, nrandom, refine);
			break;
	}
}

namespace
{
	inline int diffuse(int *diff, int src, int shift)
	{
		const int maxval = (1 << (8 - shift)) - 1;
		src += *diff;
		int ret = max(0, min(src >> shift, maxval));
		// simulate decoding ("loop filter")
		int loop = (ret << shift) | (ret >> (8 - 2 * shift));
		*diff = src - loop;
		return ret;
	}
	inline int diffuse1(int *diff, int src)
	{
		src += *diff;
		int ret = (src >= 128);
		// simulate decoding ("loop filter")
		int loop = ret ? 255 : 0;
		*diff = src - loop;
		return ret;
	}

	inline int floyd(int *thisrow, int *downrow, int src, int shift)
	{
		const int maxval = (1 << (8 - shift)) - 1;
		src = (src << 4) | (src >> 4);
		src += thisrow[1];
		int ret = max(0, min(src >> (shift + 4), maxval));
		// simulate decoding ("loop filter")
		int loop = (ret * 4095 / maxval);
		int err = src - loop;
		int e7 = (err * 7 + 8) / 16;
		err -= e7;
		int e3 = (err * 3 + 4) / 9;
		err -= e3;
		int e5 = (err * 5 + 3) / 6;
		err -= e5;
		int e1 = err;
		thisrow[2] += e7;
		downrow[0] += e3;
		downrow[1] += e5;
		downrow[2] += e1;
		return ret;
	}

	inline int floyd1(int *thisrow, int *downrow, int src)
	{
		src = (src << 4) | (src >> 4);
		src += thisrow[1];
		int ret = (src >= 2048);
		// simulate decoding ("loop filter")
		int loop = ret ? 4095 : 0;
		int err = src - loop;
		int e7 = (err * 7 + 8) / 16;
		err -= e7;
		int e3 = (err * 3 + 4) / 9;
		err -= e3;
		int e5 = (err * 5 + 3) / 6;
		err -= e5;
		int e1 = err;
		thisrow[2] += e7;
		downrow[0] += e3;
		downrow[1] += e5;
		downrow[2] += e1;
		return ret;
	}

	template<int srccomps, bool bgr, int alphabits, DitherMode dither>
	inline void rgb565_image(unsigned char *out, const unsigned char *rgba, int w, int h)
	{
		int x, y;
		switch(dither)
		{
			case DITHER_NONE:
				{
					if(bgr)
					{
						for(y = 0; y < h; ++y)
							for(x = 0; x < w; ++x)
							{
								out[(x + y * w) * 4 + 2] = rgba[(x + y * w) * srccomps + 2] >> 3;
								out[(x + y * w) * 4 + 1] = rgba[(x + y * w) * srccomps + 1] >> 2;
								out[(x + y * w) * 4 + 0] = rgba[(x + y * w) * srccomps + 0] >> 3;
							}
					}
					else
					{
						for(y = 0; y < h; ++y)
							for(x = 0; x < w; ++x)
							{
								out[(x + y * w) * 4 + 2] = rgba[(x + y * w) * srccomps + 0] >> 3;
								out[(x + y * w) * 4 + 1] = rgba[(x + y * w) * srccomps + 1] >> 2;
								out[(x + y * w) * 4 + 0] = rgba[(x + y * w) * srccomps + 2] >> 3;
							}
					}
					if(srccomps == 4)
					{
						if(alphabits == 1)
						{
							for(y = 0; y < h; ++y)
								for(x = 0; x < w; ++x)
									out[(x + y * w) * 4 + 3] = rgba[(x + y * w) * srccomps + 3] >> 7;
						}
						else if(alphabits == 8)
						{
							for(y = 0; y < h; ++y)
								for(x = 0; x < w; ++x)
									out[(x + y * w) * 4 + 3] = rgba[(x + y * w) * srccomps + 3]; // no conversion
						}
						else
						{
							int alphadiffuse = 8 - alphabits;
							for(y = 0; y < h; ++y)
								for(x = 0; x < w; ++x)
									out[(x + y * w) * 4 + 3] = rgba[(x + y * w) * srccomps + 3] >> (8 - alphabits);
						}
					}
					else
					{
						for(y = 0; y < h; ++y)
							for(x = 0; x < w; ++x)
								out[(x + y * w) * 4 + 3] = (1 << alphabits) - 1;
					}
				}
				break;
			case DITHER_SIMPLE:
				{
					int x, y;
					int diffuse_r = 0;
					int diffuse_g = 0;
					int diffuse_b = 0;
					int diffuse_a = 0;
					if(bgr)
					{
						for(y = 0; y < h; ++y)
							for(x = 0; x < w; ++x)
							{
								out[(x + y * w) * 4 + 2] = diffuse(&diffuse_r, rgba[(x + y * w) * srccomps + 2], 3);
								out[(x + y * w) * 4 + 1] = diffuse(&diffuse_g, rgba[(x + y * w) * srccomps + 1], 2);
								out[(x + y * w) * 4 + 0] = diffuse(&diffuse_b, rgba[(x + y * w) * srccomps + 0], 3);
							}
					}
					else
					{
						for(y = 0; y < h; ++y)
							for(x = 0; x < w; ++x)
							{
								out[(x + y * w) * 4 + 2] = diffuse(&diffuse_r, rgba[(x + y * w) * srccomps + 0], 3);
								out[(x + y * w) * 4 + 1] = diffuse(&diffuse_g, rgba[(x + y * w) * srccomps + 1], 2);
								out[(x + y * w) * 4 + 0] = diffuse(&diffuse_b, rgba[(x + y * w) * srccomps + 2], 3);
							}
					}
					if(srccomps == 4)
					{
						if(alphabits == 1)
						{
							for(y = 0; y < h; ++y)
								for(x = 0; x < w; ++x)
									out[(x + y * w) * 4 + 3] = diffuse1(&diffuse_a, rgba[(x + y * w) * srccomps + 3]);
						}
						else if(alphabits == 8)
						{
							for(y = 0; y < h; ++y)
								for(x = 0; x < w; ++x)
									out[(x + y * w) * 4 + 3] = rgba[(x + y * w) * srccomps + 3]; // no conversion
						}
						else
						{
							for(y = 0; y < h; ++y)
								for(x = 0; x < w; ++x)
									out[(x + y * w) * 4 + 3] = diffuse(&diffuse_a, rgba[(x + y * w) * srccomps + 3], 8 - alphabits);
						}
					}
					else
					{
						for(y = 0; y < h; ++y)
							for(x = 0; x < w; ++x)
								out[(x + y * w) * 4 + 3] = (1 << alphabits) - 1;
					}
				}
				break;
			case DITHER_FLOYDSTEINBERG:
				{
					int x, y;
					int pw = w+2;
					int downrow[6*pw];
					memset(downrow, 0, sizeof(downrow));
					int *thisrow_r, *thisrow_g, *thisrow_b, *thisrow_a;
					int *downrow_r, *downrow_g, *downrow_b, *downrow_a;
					if(bgr)
					{
						for(y = 0; y < h; ++y)
						{
							thisrow_r = downrow + ((y&1)?3:0) * pw;
							downrow_r = downrow + ((y&1)?0:3) * pw;
							memset(downrow_r, 0, sizeof(*downrow_r) * (3*pw));
							thisrow_g = thisrow_r + pw;
							thisrow_b = thisrow_g + pw;
							downrow_g = downrow_r + pw;
							downrow_b = downrow_g + pw;
							for(x = 0; x < w; ++x)
							{
								out[(x + y * w) * 4 + 2] = floyd(&thisrow_r[x], &downrow_r[x], rgba[(x + y * w) * srccomps + 2], 3);
								out[(x + y * w) * 4 + 1] = floyd(&thisrow_g[x], &downrow_g[x], rgba[(x + y * w) * srccomps + 1], 2);
								out[(x + y * w) * 4 + 0] = floyd(&thisrow_b[x], &downrow_b[x], rgba[(x + y * w) * srccomps + 0], 3);
							}
						}
					}
					else
					{
						for(y = 0; y < h; ++y)
						{
							thisrow_r = downrow + ((y&1)?3:0) * pw;
							downrow_r = downrow + ((y&1)?0:3) * pw;
							memset(downrow_r, 0, sizeof(*downrow_r) * (3*pw));
							thisrow_g = thisrow_r + pw;
							thisrow_b = thisrow_g + pw;
							downrow_g = downrow_r + pw;
							downrow_b = downrow_g + pw;
							for(x = 0; x < w; ++x)
							{
								out[(x + y * w) * 4 + 2] = floyd(&thisrow_r[x], &downrow_r[x], rgba[(x + y * w) * srccomps + 0], 3);
								out[(x + y * w) * 4 + 1] = floyd(&thisrow_g[x], &downrow_g[x], rgba[(x + y * w) * srccomps + 1], 2);
								out[(x + y * w) * 4 + 0] = floyd(&thisrow_b[x], &downrow_b[x], rgba[(x + y * w) * srccomps + 2], 3);
							}
						}
					}
					if(srccomps == 4)
					{
						if(alphabits == 1)
						{
							for(y = 0; y < h; ++y)
							{
								thisrow_a = downrow + (y&1) * pw;
								downrow_a = downrow + !(y&1) * pw;
								memset(downrow_a, 0, sizeof(*downrow_a) * pw);
								for(x = 0; x < w; ++x)
									out[(x + y * w) * 4 + 3] = floyd1(&thisrow_a[x], &downrow_a[x], rgba[(x + y * w) * srccomps + 3]);
							}
						}
						else if(alphabits == 8)
						{
							for(y = 0; y < h; ++y)
								for(x = 0; x < w; ++x)
									out[(x + y * w) * 4 + 3] = rgba[(x + y * w) * srccomps + 3]; // no conversion
						}
						else
						{
							for(y = 0; y < h; ++y)
							{
								thisrow_a = downrow + (y&1) * pw;
								downrow_a = downrow + !(y&1) * pw;
								memset(downrow_a, 0, sizeof(*downrow_a) * pw);
								for(x = 0; x < w; ++x)
									out[(x + y * w) * 4 + 3] = floyd(&thisrow_a[x], &downrow_a[x], rgba[(x + y * w) * srccomps + 3], 8 - alphabits);
							}
						}
					}
					else
					{
						for(y = 0; y < h; ++y)
							for(x = 0; x < w; ++x)
								out[(x + y * w) * 4 + 3] = (1 << alphabits) - 1;
					}
				}
				break;
		}
	}

	template<int srccomps, bool bgr, int alphabits>
	void rgb565_image(unsigned char *out, const unsigned char *rgba, int w, int h, DitherMode dither)
	{
		switch(dither)
		{
			case DITHER_NONE:
				rgb565_image<srccomps, bgr, alphabits, DITHER_NONE>(out, rgba, w, h);
				break;
			default:
			case DITHER_SIMPLE:
				rgb565_image<srccomps, bgr, alphabits, DITHER_SIMPLE>(out, rgba, w, h);
				break;
			case DITHER_FLOYDSTEINBERG:
				rgb565_image<srccomps, bgr, alphabits, DITHER_FLOYDSTEINBERG>(out, rgba, w, h);
				break;
		}
	}

	template<int srccomps, bool bgr>
	void rgb565_image(unsigned char *out, const unsigned char *rgba, int w, int h, int alphabits, DitherMode dither)
	{
		switch(alphabits)
		{
			case 1:
				rgb565_image<srccomps, bgr, 1>(out, rgba, w, h, dither);
				break;
			case 4:
				rgb565_image<srccomps, bgr, 4>(out, rgba, w, h, dither);
				break;
			default:
			case 8:
				rgb565_image<srccomps, bgr, 8>(out, rgba, w, h, dither);
				break;
		}
	}

	template<int srccomps>
	void rgb565_image(unsigned char *out, const unsigned char *rgba, int w, int h, bool bgr, int alphabits, DitherMode dither)
	{
		if(bgr)
			rgb565_image<srccomps, true>(out, rgba, w, h, alphabits, dither);
		else
			rgb565_image<srccomps, false>(out, rgba, w, h, alphabits, dither);
	}
};

void rgb565_image(unsigned char *out, const unsigned char *rgba, int w, int h, int srccomps, bool bgr, int alphabits, DitherMode dither)
{
	switch(srccomps)
	{
		case 3:
			rgb565_image<3>(out, rgba, w, h, bgr, alphabits, dither);
		case 4:
		default:
			rgb565_image<4>(out, rgba, w, h, bgr, alphabits, dither);
	}
}
