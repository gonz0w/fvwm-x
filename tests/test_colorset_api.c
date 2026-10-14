/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "libs/Colorset.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	assert(ColorsetCount() == 0);
	assert(!AllocColorset(-1));
	assert(ColorsetCount() == 0 && FindColorset(-1) == NULL);
	assert(AllocColorset(0));
	colorset_t *zero = FindColorset(0);
	assert(zero != NULL && zero->pixmap == None);
	zero->fg = 42;
	zero->bg = 100;
	zero->fg_alpha_percent = 100;
	if (FVWM_COLORSET_LIMIT > 1)
	{
		assert(AllocColorset(INT_MAX));
		colorset_t *high = FindColorset(INT_MAX);
		assert(high != NULL && high != zero && high->fg == 42);
		assert(ColorsetCount() == 2);
		assert(LoadColorset(DumpColorset(INT_MAX, high) + 8) == INT_MAX);
		assert(FindColorset(INT_MAX) == high);
	}
	/* Large valid image/shape extents must survive the text packet,
	 * including the boundary formerly truncated by 12-bit storage. */
	zero->width = 4096;
	zero->height = UINT16_MAX;
	zero->shape_width = 32768;
	zero->shape_height = 16384;
	assert(LoadColorset(DumpColorset(0, zero) + 8) == 0);
	assert(zero->width == 4096 && zero->height == UINT16_MAX &&
	    zero->shape_width == 32768 && zero->shape_height == 16384);
	const char *invalid_dimensions[] = {
	    "0 2a 64 0 0 0 0 0 0 0 64 10000 1 0 1 1 0 0 0 0 64",
	    "0 2a 64 0 0 0 0 0 0 0 64 1 10000 0 1 1 0 0 0 0 64",
	    "0 2a 64 0 0 0 0 0 0 0 64 1 1 0 10000 1 0 0 0 0 64",
	    "0 2a 64 0 0 0 0 0 0 0 64 1 1 0 1 ffffffff 0 0 0 0 64"};

	for (size_t i = 0; i < sizeof(invalid_dimensions) /
	    sizeof(invalid_dimensions[0]); i++)
	{
		assert(LoadColorset((char *)invalid_dimensions[i]) == -1);
		assert(zero->width == 4096 && zero->height == UINT16_MAX &&
		    zero->shape_width == 32768 && zero->shape_height == 16384);
	}

	/* Values beyond the scanf destination widths must not wrap into a
	 * seemingly valid extent/resource before the range checks. */
	assert(LoadColorset(
	    "0 2a 64 0 0 0 0 0 0 0 64 100000000 1 0 1 1 0 0 0 0 64") == -1);
	assert(LoadColorset(
	    "0 10000000000000000 64 0 0 0 0 0 0 0 64 1 1 0 1 1 0 0 0 0 64")
	    == -1);
	assert(LoadColorset(
	    "0 -1 64 0 0 0 0 0 0 0 64 1 1 0 1 1 0 0 0 0 64") == -1);
	assert(LoadColorset(
	    "0 2ag 64 0 0 0 0 0 0 0 64 1 1 0 1 1 0 0 0 0 64") == -1);
	assert(LoadColorset(
	    "0 2a 64 0 0 0 0 0 0 0 64 1 1 0 1 1 0 0 0 0 64g") == -1);
	assert(zero->fg == 42 && zero->width == 4096);

	size_t before = ColorsetCount();
	char malformed[] = "80000000 0 0 0 0 0 0 0 0 0 64 0 0 0 0 0 0 0 0 0 64";
	assert(LoadColorset(malformed) == -1);
	assert(LoadColorset(NULL) == -1);
	assert(LoadColorset("1 0") == -1);
	assert(ColorsetCount() == before && zero->fg == 42);
	assert(LoadColorset(
	    "3 2a 64 0 0 0 0 0 0 0 64 100000000 1 0 1 1 0 0 0 0 64") == -1);
	assert(ColorsetCount() == before && FindColorset(3) == NULL);
	for (int id = 1; ColorsetCount() < FVWM_COLORSET_LIMIT; id++)
	{
		assert(AllocColorset(id));
		assert(GetColorset(id)->fg == 42);
	}
	assert(ColorsetCount() == FVWM_COLORSET_LIMIT);
	assert(!AllocColorset(INT_MAX - 1));
	assert(FindColorset(INT_MAX - 1) == NULL);
	assert(GetColorset(INT_MAX - 1) == zero);
	assert(AllocColorset(0) && FindColorset(0) == zero);
	zero->bg = 200;
	assert(LoadColorset(DumpColorset(0, zero) + 8) == 0);
	assert(zero->bg == 200 && ColorsetCount() == FVWM_COLORSET_LIMIT);
	int id = -1;
	assert(GetColorsetArgument("2147483648", NULL, &id) == 0 && id == -1);
	assert(GetColorsetArgument("100000 tail", NULL, &id) == 1 && id == 100000);
	assert(GetColorsetArgument("100000bad", NULL, &id) == 0 && id == 100000);
	for (size_t i = 1; i < ColorsetCount(); i++)
	{
		assert(ColorsetIdAt(i - 1) < ColorsetIdAt(i));
	}
	puts("colorset API: passed");
	return 0;
}
