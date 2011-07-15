#ifndef S2TC_COMMON_H
#define S2TC_COMMON_H

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

inline int byteidx(int bit)
{
	return bit >> 3;
}

inline int bitidx(int bit)
{
	return bit & 7;
}

inline void setbit(unsigned char *arr, int bit, int v = 1)
{
	arr[byteidx(bit)] |= (v << bitidx(bit));
}

inline void xorbit(unsigned char *arr, int bit, int v = 1)
{
	arr[byteidx(bit)] ^= (v << bitidx(bit));
}

inline int testbit(const unsigned char *arr, int bit, int v = 1)
{
	return (arr[byteidx(bit)] & (v << bitidx(bit)));
}

#endif
