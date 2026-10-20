#ifndef FVWM_STYLE_FLAGS_MUTATE_H
#define FVWM_STYLE_FLAGS_MUTATE_H

#include <stdint.h>
#include <string.h>
#include "style_types.h"

/* Operations own complete style flag records. The destination may be either
 * input, or a distinct record; shifted partial overlap is not supported.
 * Read both word operands before writing, preserving exact input aliasing.
 * memcpy handles the object representation without alignment/alias casts.
 * The byte tail also covers future record sizes that are not word multiples. */
static inline void style_flags_union(style_flags *dest,
	const style_flags *first, const style_flags *second)
{
	unsigned char *d = (unsigned char *)dest;
	const unsigned char *a = (const unsigned char *)first;
	const unsigned char *b = (const unsigned char *)second;
	size_t i = 0;

	for (; sizeof(*dest) - i >= sizeof(uint64_t); i += sizeof(uint64_t))
	{
		uint64_t x, y, result;

		memcpy(&x, a + i, sizeof(x));
		memcpy(&y, b + i, sizeof(y));
		result = x | y;
		memcpy(d + i, &result, sizeof(result));
	}
	for (; i < sizeof(*dest); i++)
		d[i] = a[i] | b[i];
}

static inline void style_flags_intersection(style_flags *dest,
	const style_flags *first, const style_flags *second)
{
	unsigned char *d = (unsigned char *)dest;
	const unsigned char *a = (const unsigned char *)first;
	const unsigned char *b = (const unsigned char *)second;
	size_t i = 0;

	for (; sizeof(*dest) - i >= sizeof(uint64_t); i += sizeof(uint64_t))
	{
		uint64_t x, y, result;

		memcpy(&x, a + i, sizeof(x));
		memcpy(&y, b + i, sizeof(y));
		result = x & y;
		memcpy(d + i, &result, sizeof(result));
	}
	for (; i < sizeof(*dest); i++)
		d[i] = a[i] & b[i];
}

static inline void style_flags_without(style_flags *dest,
	const style_flags *first, const style_flags *second)
{
	unsigned char *d = (unsigned char *)dest;
	const unsigned char *a = (const unsigned char *)first;
	const unsigned char *b = (const unsigned char *)second;
	size_t i = 0;

	for (; sizeof(*dest) - i >= sizeof(uint64_t); i += sizeof(uint64_t))
	{
		uint64_t x, y, result;

		memcpy(&x, a + i, sizeof(x));
		memcpy(&y, b + i, sizeof(y));
		result = x & ~y;
		memcpy(d + i, &result, sizeof(result));
	}
	for (; i < sizeof(*dest); i++)
		d[i] = a[i] & ~b[i];
}

#endif
