/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "modules/FvwmForm/form_scale.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

static void test_metrics(void)
{
	form_scale_metrics metrics, before;
	int extent;

	for (unsigned int quarters = 4; quarters <= 16; quarters++)
	{
		assert(form_scale_metrics_prepare(quarters, 7, &metrics));
		assert(metrics.text == (int)(3 * quarters + 2) / 4);
		assert(metrics.box == metrics.text);
		assert(metrics.horizontal == (int)(10 * quarters + 2) / 4);
		assert(metrics.vertical == (int)(5 * quarters + 2) / 4);
		assert(metrics.gutter == (int)(16 * quarters) / 4);
		assert(metrics.thumb_width == (int)(10 * quarters + 2) / 4);
		assert(metrics.thumb_inset == (int)(2 * quarters + 2) / 4);
		assert(metrics.thumb_min == (int)quarters);
		assert(metrics.pad_text == (int)(7 * quarters + 2) / 4);
	}
	before = metrics;
	assert(!form_scale_metrics_prepare(3, 7, &metrics));
	assert(!form_scale_metrics_prepare(17, 7, &metrics));
	assert(!form_scale_metrics_prepare(6, -1, &metrics));
	assert(!form_scale_metrics_prepare(16, INT_MAX, &metrics));
	assert(memcmp(&before, &metrics, sizeof(metrics)) == 0);
	assert(form_scale_extent(100, 140, 5, &extent) && extent == 140);
	assert(form_scale_extent(100, 110, 8, &extent) && extent == 200);
	assert(form_scale_extent(0, 0, 4, &extent) && extent == 0);
	assert(!form_scale_extent(-1, 0, 4, &extent) && extent == 0);
	assert(!form_scale_extent(100, 32768, 4, &extent) && extent == 0);
	assert(!form_scale_extent(32767, 0, 5, &extent) && extent == 0);
	assert(!form_scale_extent(INT_MAX, 0, 16, &extent) && extent == 0);
}

static void test_cells(void)
{
	form_scale_metrics metrics;
	int cell, height;

	assert(form_scale_metrics_prepare(4, 0, &metrics));
	assert(form_scale_cell(4, 12, 12, 18, false, &metrics, &cell, &height));
	assert(cell == 108 && height == 18);
	assert(form_scale_cell(4, 12, 12, 18, true, &metrics, &cell, &height));
	assert(cell == 108 && height == 48);
	assert(form_scale_metrics_prepare(6, 0, &metrics));
	assert(form_scale_cell(6, 12, 18, 18, true, &metrics, &cell, &height));
	assert(cell == 162 && height == 74);
	/* A fixed alias can keep its glyph metrics; logical geometry grows. */
	assert(form_scale_metrics_prepare(8, 0, &metrics));
	assert(form_scale_cell(8, 12, 12, 18, true, &metrics, &cell, &height));
	assert(cell == 216 && height == 96);
	assert(form_scale_cell(8, 12, 12, 18, false, &metrics, &cell, &height));
	assert(cell == 216 && height == 36);
	/* Returned font metrics can exceed the rounded logical expectation. */
	assert(form_scale_metrics_prepare(5, 0, &metrics));
	assert(form_scale_cell(5, 13, 17, 18, false, &metrics, &cell, &height));
	assert(cell == 153 && height >= 25);
	/* Every crossing starts from 1x, avoiding accumulated rounding. */
	for (int pass = 0; pass < 100; pass++)
	{
		assert(form_scale_metrics_prepare(4, 0, &metrics));
		assert(form_scale_cell(4, 13, 13, 18, false,
			&metrics, &cell, &height));
		assert(cell == 117 && height == 19);
		assert(form_scale_metrics_prepare(5, 0, &metrics));
		assert(form_scale_cell(5, 13, 16, 18, false,
			&metrics, &cell, &height));
		assert(cell == 146 && height == 24);
	}
	assert(!form_scale_cell(5, 13, 16, 0, false,
		&metrics, &cell, &height) && cell == 0 && height == 0);
	assert(!form_scale_cell(5, 13, 16, 4097, false,
		&metrics, &cell, &height));
	assert(!form_scale_cell(5, INT_MAX, INT_MAX, 4096, true,
		&metrics, &cell, &height));
	assert(!form_scale_cell(5, 13, -1, 18, false,
		&metrics, &cell, &height));
	assert(!form_scale_cell(5, 13, 16, 18, false,
		NULL, &cell, &height));
}

int main(void)
{
	test_metrics();
	test_cells();
	return 0;
}
