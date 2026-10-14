/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/font_shadow.h"

#include <assert.h>
#include <limits.h>

static void test_counts(void)
{
	uint32_t count = 9;
	assert(font_shadow_count(31, 0, 0, true, 4096, &count));
	assert(count == 3968);
	assert(!font_shadow_count(32, 0, 0, true, 4096, &count));
	assert(count == 0);
	assert(font_shadow_count(64, 0, 0, true, 65536, &count));
	assert(count == 16640);
	assert(font_shadow_count(512, 0, 8, false, 4096, &count));
	assert(count == 4096);
	assert(!font_shadow_count(513, 0, 8, false, 4096, &count));
	assert(font_shadow_count(1, 8191, 0, true, 65536, &count));
	assert(count == 65536);
	assert(!font_shadow_count(1, 8192, 0, true, 65536, &count));
	assert(font_shadow_count(0, INT_MAX, 8, true, 1, &count));
	assert(count == 0);
	assert(font_shadow_count(1, 0, 1, false, 1, &count));
	assert(count == 1);
	assert(!font_shadow_count(1, 0, 0, true, 1, &count));
	assert(font_shadow_count(2, 1, 3, true, 46, &count));
	assert(count == 46);
	assert(!font_shadow_count(2, 1, 3, true, 45, &count));
	assert(!font_shadow_count(-1, 0, 1, false, 4096, &count));
	assert(!font_shadow_count(1, -1, 1, false, 4096, &count));
	assert(!font_shadow_count(1, 0, 9, false, 4096, &count));
	assert(!font_shadow_count(1, 0, 1, false, 0, &count));
	assert(!font_shadow_count(1, 0, 1, false, 65537, &count));
	assert(!font_shadow_count(INT_MAX, INT_MAX, 8, true, 65536, &count));
	assert(!font_shadow_count(INT_MAX, 0, 0, true, 65536, &count));
	assert(!font_shadow_count(32768, 0, 0, true, 65536, &count));
	assert(!font_shadow_count(1, INT_MAX, 0, true, 65536, &count));
	assert(count == 0);
}

static void test_legacy_border_order(void)
{
	for (int size = 1; size <= 12; size++)
	{
		for (int offset = 0; offset <= 8; offset++)
		{
			uint32_t index = 0;
			uint32_t count;
			int x, y;
			int outer = size + offset;
			assert(font_shadow_count(size, offset, 0, true,
			    65536, &count));
			for (int ty = -outer; ty <= outer; ty++)
			{
				for (int tx = -outer; tx <= outer; tx++)
				{
					if (tx > -offset - 1 && tx < offset + 1 &&
					    ty > -offset - 1 && ty < offset + 1)
					{
						continue;
					}
					assert(font_shadow_center_position(size,
					    offset, index++, &x, &y));
					assert(x == tx && y == ty);
				}
			}
			assert(index == count);
			assert(!font_shadow_center_position(size, offset,
			    index, &x, &y));
			assert(x == 0 && y == 0);
		}
	}
}

static void test_sparse_border_and_endpoints(void)
{
	int x, y;
	/* A wide excluded interior still needs exactly 65536 constant-work
	 * positions; no square-size counter or skipped-pixel traversal. */
	for (uint32_t index = 0; index < 65536; index++)
	{
		assert(font_shadow_center_position(1, 8191, index, &x, &y));
		assert(x == -8192 || x == 8192 || y == -8192 || y == 8192);
	}
	assert(!font_shadow_center_position(INT_MAX, INT_MAX, 0, &x, &y));
	assert(!font_shadow_center_position(INT_MIN, INT_MIN, UINT32_MAX, &x, &y));
	assert(!font_shadow_center_position(0, 0, 0, &x, &y));
	assert(x == 0 && y == 0);
}

int main(void)
{
	test_counts();
	test_legacy_border_order();
	test_sparse_border_and_endpoints();
	return 0;
}
