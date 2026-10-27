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
		assert(metrics.stroke == (int)(quarters + 2) / 4);
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

static void test_label_geometry(void)
{
	int width;
	int minimum;

	assert(form_scale_extent(100, 0, 6, &minimum) && minimum == 150);
	for (int i = 0; i < 100; i++)
	{
		assert(form_label_viewport_width(minimum, 1000, 15, &width));
		assert(width == 970 && minimum == 150);
		assert(form_label_viewport_width(minimum, 180, 15, &width));
		assert(width == 150 && minimum == 150);
	}
	assert(!form_label_viewport_width(minimum, 179, 15, &width));
	assert(width == 0 && minimum == 150);
	assert(!form_label_viewport_width(INT_MAX, 32767, 10, &width));
	assert(!form_label_viewport_width(1, INT_MAX, 10, &width));
	assert(!form_label_viewport_width(1, 32767, INT_MAX, &width));
	assert(!form_label_viewport_width(1, 32767, -1, &width));
	assert(!form_label_viewport_width(0, 100, 1, &width));
	assert(form_label_viewport_width(32767, 32767, 0, &width));
	assert(width == 32767);
}

typedef struct
{
	int high_bytes, calls;
	bool kern, refuse;
} label_measurement;

/* Test boundaries deliberately include a non-UTF-8 two-byte font encoding. */
static int label_next(void *context, const char *text)
{
	label_measurement *measurement = context;

	return (unsigned char)*text >= 128 ? measurement->high_bytes : 1;
}

static int label_width(void *context, const char *text, int length)
{
	label_measurement *measurement = context;
	int width = 0;

	measurement->calls++;
	if (measurement->refuse)
		return -1;
	for (int i = 0; i < length; )
	{
		int next = label_next(context, text + i);

		if (next < 1)
			return 10;
		i += next;
		width += 10;
	}
	/* A longer shaped prefix can fit after a shorter prefix failed. */
	if (measurement->kern && length == 5 && memcmp(text, "ab...", 5) == 0)
		width = 35;
	return width;
}

static void test_label_text(void)
{
	label_measurement measurement = {.high_bytes = 2};
	char display[FORM_LABEL_BYTES + 4];
	char source[FORM_LABEL_BYTES + 2];
	int length;

	assert(form_label_fit("abcdef", 6, 60, &measurement,
		label_next, label_width, display, &length));
	assert(length == 6 && strcmp(display, "abcdef") == 0);
	assert(form_label_fit("abcdef", 6, 50, &measurement,
		label_next, label_width, display, &length));
	assert(length == 5 && strcmp(display, "ab...") == 0);
	assert(form_label_fit("abcdef", 6, 30, &measurement,
		label_next, label_width, display, &length));
	assert(length == 3 && strcmp(display, "...") == 0);
	assert(form_label_fit("abcdef", 6, 29, &measurement,
		label_next, label_width, display, &length));
	assert(length == 0 && display[0] == '\0');
	measurement.kern = true;
	assert(form_label_fit("abcdef", 6, 35, &measurement,
		label_next, label_width, display, &length));
	assert(length >= 3 && label_width(&measurement, display, length) <= 35);
	measurement.kern = false;
	/* One complete legacy font character, then the suffix. */
	assert(form_label_fit("\x81\x82" "abcd", 6, 40, &measurement,
		label_next, label_width, display, &length));
	assert(length == 5 && memcmp(display, "\x81\x82...", 6) == 0);
	measurement.high_bytes = 3;
	assert(form_label_fit("\xe2\x82\xac" "abcd", 7, 40, &measurement,
		label_next, label_width, display, &length));
	assert(length == 6 && memcmp(display, "\xe2\x82\xac...", 7) == 0);
	measurement.high_bytes = 4;
	assert(form_label_fit("\xf0\x9f\x98\x80" "abcd", 8, 40, &measurement,
		label_next, label_width, display, &length));
	assert(length == 7 && memcmp(display, "\xf0\x9f\x98\x80...", 8) == 0);
	/* A bad boundary cannot publish a partial character or stale buffer. */
	assert(!form_label_fit("\x81", 1, 0, &measurement,
		label_next, label_width, display, &length));
	assert(length == 0 && display[0] == '\0');
	measurement.high_bytes = 0;
	assert(!form_label_fit("\x81", 1, 0, &measurement,
		label_next, label_width, display, &length));
	measurement.high_bytes = 2;
	assert(form_label_fit("", 0, 0, &measurement,
		label_next, label_width, display, &length));
	assert(length == 0 && display[0] == '\0');
	memset(source, 'a', sizeof(source));
	source[FORM_LABEL_BYTES] = '\0';
	measurement.calls = 0;
	assert(form_label_fit(source, FORM_LABEL_BYTES, 50, &measurement,
		label_next, label_width, display, &length));
	assert(length == 5 && strcmp(display, "aa...") == 0);
	assert(measurement.calls <= 12);
	assert(source[0] == 'a' && source[FORM_LABEL_BYTES - 1] == 'a');
	assert(!form_label_fit(source, FORM_LABEL_BYTES + 1, 50, &measurement,
		label_next, label_width, display, &length));
	assert(length == 0 && display[0] == '\0');
	assert(!form_label_fit(source, -1, 50, &measurement,
		label_next, label_width, display, &length));
	assert(!form_label_fit(source, 1, -1, &measurement,
		label_next, label_width, display, &length));
	measurement.refuse = true;
	assert(!form_label_fit(source, 1, 100, &measurement,
		label_next, label_width, display, &length));
	assert(length == 0 && display[0] == '\0');
}

static void test_focus_geometry(void)
{
	form_caret_rectangle rectangle;

	assert(form_focus_stroke(0, 100, 20) == 1);
	assert(form_focus_stroke(2, 100, 20) == 2);
	assert(form_focus_stroke(INT_MAX, 100, 20) == 4);
	assert(form_focus_stroke(4, 1, 20) == 1);
	assert(form_focus_stroke(4, 2, 20) == 1);
	assert(form_focus_stroke(4, 3, 20) == 2);
	assert(form_focus_stroke(4, 0, 20) == 0);
	assert(form_focus_stroke(4, 100, -1) == 0);
	assert(form_caret_geometry(5, 100, 24, 3, 1, &rectangle));
	assert(rectangle.x == 5 && rectangle.y == 3 &&
		rectangle.width == 1 && rectangle.height == 18);
	assert(form_caret_geometry(5, 100, 24, 3, 2, &rectangle));
	assert(rectangle.x == 5 && rectangle.width == 2);
	assert(form_caret_geometry(INT_MAX, 100, 24, 3, 4, &rectangle));
	assert(rectangle.x == 93 && rectangle.width == 4);
	assert(form_caret_geometry(INT_MIN, 100, 24, 3, 4, &rectangle));
	assert(rectangle.x == 3);
	assert(form_caret_geometry(5, 7, 24, 3, 4, &rectangle));
	assert(rectangle.x == 3 && rectangle.width == 1);
	assert(!form_caret_geometry(5, 6, 24, 3, 4, &rectangle));
	assert(rectangle.width == 0 && rectangle.height == 0);
	assert(!form_caret_geometry(5, 100, 24, INT_MAX, 4, &rectangle));
	assert(!form_caret_geometry(5, 100, 24, -1, 4, &rectangle));
}

int main(void)
{
	test_metrics();
	test_cells();
	test_label_geometry();
	test_label_text();
	test_focus_geometry();
	return 0;
}
