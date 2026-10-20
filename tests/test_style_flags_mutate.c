/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "fvwm/style_flags_mutate.h"

typedef void (*operation)(style_flags *, const style_flags *,
	const style_flags *);

/* Independent references retain the old plain-char byte operations. The
 * old reduction values were unused at every shipped mutable call site. */
static void reference_union(style_flags *dest, const style_flags *first,
	const style_flags *second)
{
	char *d = (char *)dest;
	const char *a = (const char *)first, *b = (const char *)second;

	for (size_t i = 0; i < sizeof(*dest); i++)
		d[i] = a[i] | b[i];
}

static void reference_intersection(style_flags *dest, const style_flags *first,
	const style_flags *second)
{
	char *d = (char *)dest;
	const char *a = (const char *)first, *b = (const char *)second;

	for (size_t i = 0; i < sizeof(*dest); i++)
		d[i] = a[i] & b[i];
}

static void reference_without(style_flags *dest, const style_flags *first,
	const style_flags *second)
{
	char *d = (char *)dest;
	const char *a = (const char *)first, *b = (const char *)second;

	for (size_t i = 0; i < sizeof(*dest); i++)
		d[i] = a[i] & ~b[i];
}

static operation operations[] = { style_flags_union,
	style_flags_intersection, style_flags_without };
static operation references[] = { reference_union,
	reference_intersection, reference_without };
/* All five partitions of destination, first input and second input. */
static const unsigned aliases[][3] = {
	{ 0, 1, 2 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 1, 1 }, { 0, 0, 0 }
};
static unsigned long cases;

static void compare(const style_flags *first, const style_flags *second)
{
	for (size_t alias = 0; alias < 5; alias++)
		for (size_t kind = 0; kind < 3; kind++)
		{
			style_flags actual[3], expected[3];
			unsigned d = aliases[alias][0];
			unsigned a = aliases[alias][1];
			unsigned b = aliases[alias][2];

			memset(actual, 0xa5, sizeof(actual));
			memcpy(&actual[a], first, sizeof(*first));
			memcpy(&actual[b], second, sizeof(*second));
			memcpy(expected, actual, sizeof(actual));
			references[kind](&expected[d], &expected[a],
				&expected[b]);
			operations[kind](&actual[d], &actual[a], &actual[b]);
			/* Check untouched inputs and padding too. */
			assert(memcmp(actual, expected, sizeof(actual)) == 0);
			cases++;
		}
}

static void byte_values(void)
{
	style_flags first, second;

	for (unsigned a = 0; a < 256; a++)
		for (unsigned b = 0; b < 256; b++)
		{
			memset(&first, a, sizeof(first));
			memset(&second, b, sizeof(second));
			compare(&first, &second);
		}
}

static void positions(void)
{
	style_flags first, second;
	unsigned char *a = (unsigned char *)&first;
	unsigned char *b = (unsigned char *)&second;

	for (size_t i = 0; i < sizeof(first); i++)
		for (unsigned bit = 1; bit <= 128; bit <<= 1)
		{
			memset(&first, 0, sizeof(first));
			memset(&second, 0xff, sizeof(second));
			a[i] = bit;
			b[i] = (unsigned char)~bit;
			compare(&first, &second);
			compare(&second, &first);
		}
	for (unsigned seed = 0; seed < 256; seed++)
	{
		for (size_t i = 0; i < sizeof(first); i++)
		{
			a[i] = (unsigned char)(i * 137 + seed);
			b[i] = (unsigned char)(i * 73 - seed);
		}
		compare(&first, &second);
	}
}

static void guard_case(unsigned char **maps, long page, unsigned edge,
	unsigned alias, unsigned kind)
{
	style_flags expected[3], *records[3];
	unsigned d = aliases[alias][0];
	unsigned a = aliases[alias][1];
	unsigned b = aliases[alias][2];

	for (unsigned i = 0; i < 3; i++)
	{
		assert(mprotect(maps[i] + page, page,
			PROT_READ | PROT_WRITE) == 0);
		records[i] = (style_flags *)(maps[i] + page +
			(edge ? page - sizeof(style_flags) : 0));
		assert((uintptr_t)records[i] %
			_Alignof(style_flags) == 0);
		memset(records[i], 0x37 + i * 61,
			sizeof(style_flags));
		memcpy(&expected[i], records[i],
			sizeof(style_flags));
		if (i != d)
			assert(mprotect(maps[i] + page, page,
				PROT_READ) == 0);
	}
	references[kind](&expected[d], &expected[a], &expected[b]);
	operations[kind](records[d], records[a], records[b]);
	for (unsigned i = 0; i < 3; i++)
		assert(memcmp(records[i], &expected[i],
			sizeof(style_flags)) == 0);
	cases++;
}

static void guards(void)
{
	long page = sysconf(_SC_PAGESIZE);
	unsigned char *maps[3];

	assert(page > 0 && (size_t)page >= sizeof(style_flags));
	for (unsigned i = 0; i < 3; i++)
	{
		maps[i] = mmap(NULL, (size_t)page * 3, PROT_NONE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		assert(maps[i] != MAP_FAILED);
	}
	for (unsigned edge = 0; edge < 2; edge++)
		for (unsigned alias = 0; alias < 5; alias++)
			for (unsigned kind = 0; kind < 3; kind++)
				guard_case(maps, page, edge, alias, kind);
	for (unsigned i = 0; i < 3; i++)
		assert(munmap(maps[i], (size_t)page * 3) == 0);
}

static uint64_t monotonic_ns(void)
{
	struct timespec now;

	assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
	return (uint64_t)now.tv_sec * 1000000000 + now.tv_nsec;
}

static void benchmark_sample(unsigned sample, unsigned alias,
	unsigned kind, unsigned version, operation fn)
{
	style_flags records[3];
	unsigned d = aliases[alias][0];
	unsigned a = aliases[alias][1];
	unsigned b = aliases[alias][2];
	uint64_t start, elapsed;

	memset(records, 0x55, sizeof(records));
	memset(&records[b], 0xaa, sizeof(style_flags));
	start = monotonic_ns();
	for (unsigned n = 0; n < 200000; n++)
		fn(&records[d], &records[a], &records[b]);
	elapsed = monotonic_ns() - start;
	printf("mutation-bench %u %u %u %u %zu %llu\n",
		sample, alias, kind, version,
		sizeof(style_flags),
		(unsigned long long)elapsed);
}

static void benchmark(void)
{
	/* Volatile function pointers retain every call in the paired probe.
	 * These are operation costs, not a whole-WM latency guarantee. */
	operation volatile probes[2][3] = {
		{ reference_union, reference_intersection, reference_without },
		{ style_flags_union, style_flags_intersection,
			style_flags_without }
	};

	for (unsigned sample = 0; sample < 3; sample++)
		for (unsigned alias = 0; alias < 5; alias++)
			for (unsigned kind = 0; kind < 3; kind++)
				for (unsigned turn = 0; turn < 2; turn++)
				{
					unsigned version = turn ^ (sample & 1);

					benchmark_sample(sample, alias, kind,
						version, probes[version][kind]);
				}
}

int main(void)
{
	byte_values();
	positions();
	guards();
	printf("style flag mutations: %lu reference cases, %zu bytes\n",
		cases, sizeof(style_flags));
	if (getenv("FVWM_STYLE_MUTATE_BENCH"))
		benchmark();
	return 0;
}
