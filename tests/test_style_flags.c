/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "fvwm/style_types.h"
#include "fvwm/style_flags_predicate.h"

/* Independent byte references retain the original signed-char expressions. */
static bool reference_subset(const void *first, const void *second, int length)
{
	const char *a = first, *b = second;

	for (int i = 0; i < length; i++)
		if ((a[i] & b[i]) != a[i])
			return false;
	return true;
}

static bool reference_intersect(const void *first, const void *second,
	int length)
{
	const char *a = first, *b = second;

	for (int i = 0; i < length; i++)
		if (a[i] & b[i])
			return true;
	return false;
}

static bool reference_equal(const void *first, const void *second,
	const void *mask, int length)
{
	const char *a = first, *b = second, *m = mask;

	for (int i = 0; i < length; i++)
		if ((a[i] & m[i]) != (b[i] & m[i]))
			return false;
	return true;
}

static unsigned long cases;

static void compare(const void *a, const void *b, const void *mask, int length)
{
	assert(style_flags_subset(a, b, length) ==
		reference_subset(a, b, length));
	assert(style_flags_intersect(a, b, length) ==
		reference_intersect(a, b, length));
	assert(style_flags_equal_masked(a, b, mask, length) ==
		reference_equal(a, b, mask, length));
	cases++;
}

static void byte_values(void)
{
	const unsigned char masks[] = { 0, 1, 0x55, 0x7f, 0x80, 0xaa, 0xfe, 0xff };

	for (unsigned a = 0; a < 256; a++)
		for (unsigned b = 0; b < 256; b++)
			for (size_t m = 0; m < sizeof(masks); m++)
			{
				unsigned char x = a, y = b;

				compare(&x, &y, &masks[m], 1);
			}
}

static void spans(void)
{
	unsigned char a[272], b[272], mask[272], saved[272];

	for (unsigned offset = 0; offset < 8; offset++)
		for (int length = 0; length <= 257; length++)
		{
			memset(a, 0x55, sizeof(a));
			memset(b, 0x55, sizeof(b));
			memset(mask, 0xff, sizeof(mask));
			compare(a + offset, b + 7 - offset, mask + offset,
				length);
			for (int i = 0; i < length; i++)
			{
				/* Isolate each bit in short spans and rotate the bit
				 * across every position in longer spans. */
				for (unsigned bit = 1; bit <= 128; bit <<= 1)
				{
					if (length > 17)
						bit = 1U << ((unsigned)i % 8);
					memset(a, 0, sizeof(a));
					memset(b, 0, sizeof(b));
					a[offset + i] = bit;
					compare(a + offset, b + 7 - offset,
						mask + offset, length);
					b[7 - offset + i] = bit;
					compare(a + offset, b + 7 - offset,
						mask + offset, length);
					mask[offset + i] = 0;
					b[7 - offset + i] = 0;
					compare(a + offset, b + 7 - offset,
						mask + offset, length);
					mask[offset + i] = 0xff;
					if (length > 17)
						break;
				}
			}
			for (size_t i = 0; i < sizeof(a); i++)
				a[i] = (unsigned char)(i * 137 + length);
			memcpy(saved, a, sizeof(a));
			/* Read-only overlapping input is valid, including the mask. */
			compare(a + offset, a + offset + 1, a + offset + 2,
				length);
			assert(memcmp(saved, a, sizeof(a)) == 0);
		}
}

static void guarded_spans(void)
{
	long page = sysconf(_SC_PAGESIZE);
	unsigned char *regions[3];

	assert(page > 257);
	for (unsigned i = 0; i < 3; i++)
	{
		regions[i] = mmap(NULL, (size_t)page * 3, PROT_NONE,
			MAP_PRIVATE | MAP_ANON, -1, 0);
		assert(regions[i] != MAP_FAILED);
		assert(mprotect(regions[i] + page, page,
			PROT_READ | PROT_WRITE) == 0);
	}
	for (int length = 0; length <= 257; length++)
		for (unsigned edge = 0; edge < 2; edge++)
		{
			unsigned char *a = regions[0] + page * (edge + 1);
			unsigned char *b = regions[1] + page * (edge + 1);
			unsigned char *m = regions[2] + page * (edge + 1);

			if (edge)
			{
				a -= length;
				b -= length;
				m -= length;
			}
			/* Full scans ensure all three spans respect the guard. */
			memset(a, 0, length);
			memset(b, 0, length);
			memset(m, 0xff, length);
			compare(a, b, m, length);
			if (length)
			{
				a[length - 1] = 0x80;
				compare(a, b, m, length);
				b[length - 1] = 0x80;
				compare(a, b, m, length);
			}
		}
	for (unsigned i = 0; i < 3; i++)
		assert(munmap(regions[i], (size_t)page * 3) == 0);
}

typedef bool (*predicate)(const void *, const void *, const void *, int);

static bool old_subset(const void *a, const void *b, const void *mask, int n)
{
	(void)mask;
	return reference_subset(a, b, n);
}

static bool new_subset(const void *a, const void *b, const void *mask, int n)
{
	(void)mask;
	return style_flags_subset(a, b, n);
}

static bool old_intersect(const void *a, const void *b, const void *mask, int n)
{
	(void)mask;
	return reference_intersect(a, b, n);
}

static bool new_intersect(const void *a, const void *b, const void *mask, int n)
{
	(void)mask;
	return style_flags_intersect(a, b, n);
}

static uint64_t now_ns(void)
{
	struct timespec time;

	assert(clock_gettime(CLOCK_MONOTONIC, &time) == 0);
	return (uint64_t)time.tv_sec * UINT64_C(1000000000) + time.tv_nsec;
}

static void benchmark(void)
{
	predicate functions[3][2] = {
		{ old_subset, new_subset },
		{ old_intersect, new_intersect },
		{ reference_equal, style_flags_equal_masked }
	};
	const int lengths[] = { sizeof(style_id_flags), sizeof(style_flags),
		sizeof(window_flags) };
	unsigned char a[sizeof(style_flags) + sizeof(window_flags)];
	unsigned char b[sizeof(a)], mask[sizeof(a)];
	volatile unsigned accepted = 0;

	for (unsigned kind = 0; kind < 3; kind++)
		for (unsigned width = 0; width < 3; width++)
			for (unsigned position = 0; position < 3; position++)
			{
				int n = lengths[width];

				memset(a, 0, sizeof(a));
				memset(b, 0, sizeof(b));
				memset(mask, 0xff, sizeof(mask));
				if (position)
				{
					unsigned at = position == 1 ? 0 : n - 1;

					a[at] = 0x80;
					if (kind == 1)
						b[at] = 0x80;
				}
				for (unsigned sample = 0; sample < 3; sample++)
				for (unsigned order = 0; order < 2; order++)
				{
					unsigned version = order ^ (sample & 1);
					/* Volatile function-pointer reads prevent hoisting
					 * a pure call outside the measured loop. */
					predicate volatile call = functions[kind][version];
					uint64_t start = now_ns();

					for (unsigned i = 0; i < 200000; i++)
						accepted += call(a, b, mask, n);
					printf("STYLE_FLAGS_BENCH {\"sample\":%u,\"kind\":%u,"
						"\"bytes\":%d,\"position\":%u,"
						"\"version\":%u,\"calls\":200000,"
						"\"ns\":%llu}\n", sample, kind, n, position,
						version, (unsigned long long)(now_ns() - start));
				}
			}
	assert(accepted > 0);
}

int main(void)
{
	compare(NULL, NULL, NULL, 0);
	compare(NULL, NULL, NULL, -1);
	compare(NULL, NULL, NULL, INT_MIN);
	byte_values();
	spans();
	guarded_spans();
	printf("STYLE_FLAGS_CHECK {\"cases\":%lu,\"id_bytes\":%zu,"
		"\"style_bytes\":%zu,\"window_bytes\":%zu}\n", cases,
		sizeof(style_id_flags), sizeof(style_flags), sizeof(window_flags));
	if (getenv("FVWM_STYLE_FLAGS_BENCH"))
		benchmark();
	return 0;
}
