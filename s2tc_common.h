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

#endif
