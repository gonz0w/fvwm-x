#ifndef FVWM_STYLE_FLAGS_PREDICATE_H
#define FVWM_STYLE_FLAGS_PREDICATE_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Read-only flag predicates accept overlapping and unaligned byte spans.
 * memcpy keeps word loads within the supplied length without aliasing or
 * alignment assumptions. A four-byte step covers short ID records.
 * Empty and negative lengths retain the old int-length wrapper semantics. */
static inline bool style_flags_subset(const void *sub, const void *super,
	int length)
{
	const unsigned char *a = sub, *b = super;

	while (length >= 8)
	{
		uint64_t x, y;

		memcpy(&x, a, sizeof(x));
		memcpy(&y, b, sizeof(y));
		if (x & ~y)
			return false;
		a += 8;
		b += 8;
		length -= 8;
	}
	if (length >= 4)
	{
		uint32_t x, y;

		memcpy(&x, a, sizeof(x));
		memcpy(&y, b, sizeof(y));
		if (x & ~y)
			return false;
		a += 4;
		b += 4;
		length -= 4;
	}
	for (int i = 0; i < length; i++)
		if (a[i] & (unsigned char)~b[i])
			return false;
	return true;
}

static inline bool style_flags_intersect(const void *first, const void *second,
	int length)
{
	const unsigned char *a = first, *b = second;

	while (length >= 8)
	{
		uint64_t x, y;

		memcpy(&x, a, sizeof(x));
		memcpy(&y, b, sizeof(y));
		if (x & y)
			return true;
		a += 8;
		b += 8;
		length -= 8;
	}
	if (length >= 4)
	{
		uint32_t x, y;

		memcpy(&x, a, sizeof(x));
		memcpy(&y, b, sizeof(y));
		if (x & y)
			return true;
		a += 4;
		b += 4;
		length -= 4;
	}
	for (int i = 0; i < length; i++)
		if (a[i] & b[i])
			return true;
	return false;
}

static inline bool style_flags_equal_masked(const void *first,
	const void *second, const void *mask, int length)
{
	const unsigned char *a = first, *b = second, *m = mask;

	while (length >= 8)
	{
		uint64_t x, y, bits;

		memcpy(&x, a, sizeof(x));
		memcpy(&y, b, sizeof(y));
		memcpy(&bits, m, sizeof(bits));
		if ((x ^ y) & bits)
			return false;
		a += 8;
		b += 8;
		m += 8;
		length -= 8;
	}
	if (length >= 4)
	{
		uint32_t x, y, bits;

		memcpy(&x, a, sizeof(x));
		memcpy(&y, b, sizeof(y));
		memcpy(&bits, m, sizeof(bits));
		if ((x ^ y) & bits)
			return false;
		a += 4;
		b += 4;
		m += 4;
		length -= 4;
	}
	for (int i = 0; i < length; i++)
		if ((a[i] ^ b[i]) & m[i])
			return false;
	return true;
}

#endif
