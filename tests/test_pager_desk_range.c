/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "modules/FvwmPager/pager_desk_range.h"
#include <assert.h>
#include <limits.h>

int main(void)
{
	pager_desk_range range;
	assert(pager_prepare_desk_range("0", "4095", 4096, &range));
	assert(range.count == 4096 && range.first == 0 && range.last == 4095);
	assert(!pager_prepare_desk_range("0", "4096", 4096, &range));
	assert(range.count == 0 && range.first == 0 && range.last == 0);
	assert(pager_prepare_desk_range("0", "4096", 65536, &range));
	assert(range.count == 4097);
	assert(pager_prepare_desk_range("0", "65535", 65536, &range));
	assert(range.count == 65536);
	assert(!pager_prepare_desk_range("0", "65536", 65536, &range));
	assert(pager_prepare_desk_range("2147483647", NULL, 4096, &range));
	assert(range.first == INT_MAX && range.last == INT_MAX && range.count == 1);
	assert(pager_prepare_desk_range("7suffix", "3", 4096, &range));
	assert(range.first == 3 && range.last == 7 && range.count == 5);
	assert(pager_prepare_desk_range("-7", "-2", 4096, &range));
	assert(range.first == 0 && range.last == 0 && range.count == 1);
	assert(!pager_prepare_desk_range("0", "2147483647", 4096, &range));
	assert(!pager_prepare_desk_range("2147483648", NULL, 4096, &range));
	assert(!pager_prepare_desk_range("x", NULL, 4096, &range));
	assert(!pager_prepare_desk_range(NULL, NULL, 4096, &range));
	assert(!pager_prepare_desk_range("1", NULL, 0, &range));
	assert(!pager_prepare_desk_range("1", NULL, 65537, &range));
	assert(!pager_prepare_desk_range("1", NULL, 4096, NULL));
	assert(pager_desk_index(INT_MIN, INT_MAX, 1) == -1);
	assert(pager_desk_index(INT_MAX, INT_MIN, 1) == -1);
	assert(pager_desk_index(INT_MAX, INT_MAX, 1) == 0);
	assert(pager_desk_index(INT_MIN, INT_MIN, 1) == 0);
	assert(pager_desk_index(300001, 300000, 2) == 1);
	assert(pager_desk_index(300002, 300000, 2) == -1);
	assert(pager_desk_index(-2, -3, 2) == 1);
	assert(pager_desk_index(0, 0, 0) == -1);
	assert(pager_desk_index(0, 0, -1) == -1);
	return 0;
}
