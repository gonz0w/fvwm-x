/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "modules/FvwmForm/form_scale.h"
#include "modules/FvwmForm/form_workarea.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void test_workarea(void)
{
	module_ui_scale_snapshot snapshot = {.epoch = 1, .count = 2,
		.rows = {{.name_atom = 1, .quarters = 5, .primary = true,
			.rect = {-800, 100, 800, 600}, .name = "left display"},
			{.name_atom = 2, .quarters = 8,
			.rect = {0, 0, 1200, 900}, .name = "right"}}};
	form_workarea_state state = {0};
	module_monitor_area area;
	form_scale_size minimum = {150, 80}, available, size;
	char line[MODULE_UI_SCALE_LINE_MAX];
	int position;
	int values[MODULE_MONITOR_VALUES] = {0};

	form_workarea_sync(&state, &snapshot);
	assert(form_workarea_bounds(&state, &snapshot, &snapshot.rows[0], &area));
	assert(area.x == -800 && area.y == 100);
	assert(area.width == 800 && area.height == 600);
	values[10] = 10;
	values[11] = 20;
	values[12] = 40;
	values[13] = 30;
	assert(module_monitor_format(line, sizeof(line), "left display", values));
	assert(form_workarea_receive(&state, &snapshot, line) ==
		FORM_WORKAREA_ADMITTED);
	assert(form_workarea_bounds(&state, &snapshot, &snapshot.rows[0], &area));
	assert(area.x == -790 && area.y == 140);
	assert(area.width == 770 && area.height == 530);
	/* Client preference reserves actual frame decorations from this area. */
	available = (form_scale_size){area.width - 8, area.height - 32};
	assert(form_scale_initial_size(800, 32, 5, 50, 30, 6,
		&minimum, &available, &size));
	assert(size.width == 762 && size.height == 498);
	assert(form_scale_startup_axis(100, size.width + 8,
		area.x, area.width, &position) && position == -790);
	assert(form_scale_startup_axis(0, size.height + 32,
		area.y, area.height, &position) && position == 140);
	/* Malformed input retains only a last good sample in this topology. */
	values[10] = -1;
	assert(module_monitor_format(line, sizeof(line), "left display", values));
	assert(form_workarea_receive(&state, &snapshot, line) ==
		FORM_WORKAREA_REFUSED);
	assert(form_workarea_bounds(&state, &snapshot, &snapshot.rows[0], &area));
	assert(area.x == -790 && area.width == 770);
	assert(form_workarea_receive(&state, &snapshot,
		"Monitor right 0 0") == FORM_WORKAREA_REFUSED);
	assert(form_workarea_bounds(&state, &snapshot, &snapshot.rows[1], &area));
	assert(area.x == 0 && area.width == 1200 && area.height == 900);
	assert(form_workarea_receive(&state, &snapshot,
		"MonitorUI 1 END 0000000000000001 2") == FORM_WORKAREA_IGNORED);
	assert(form_workarea_receive(&state, &snapshot,
		"Monitor unknown 0 0 0 0 0 0 0 0 0 0 0 0 0 0") ==
		FORM_WORKAREA_REFUSED);
	assert(form_workarea_receive(&state, &snapshot,
		"Monitor right 0 0 0 0 0 0 0 0 0 0 0 0 0 0 junk") ==
		FORM_WORKAREA_REFUSED);
	assert(form_workarea_receive(&state, &snapshot,
		"Monitor right 0 0 0 0 0 0 0 0 0 0 0 0 0 1x") ==
		FORM_WORKAREA_REFUSED);
	/* A changed publication invalidates even a matching name; legacy rows
	 * cannot certify freshness across a topology or scale epoch change. */
	snapshot.epoch++;
	snapshot.rows[0].rect = (rectangle){100, 50, 800, 600};
	form_workarea_sync(&state, &snapshot);
	assert(form_workarea_bounds(&state, &snapshot, &snapshot.rows[0], &area));
	assert(area.x == 100 && area.y == 50 && area.height == 600);
	values[10] = 0;
	values[11] = INT_MAX;
	assert(module_monitor_format(line, sizeof(line), "left display", values));
	assert(form_workarea_receive(&state, &snapshot, line) ==
		FORM_WORKAREA_REFUSED);
	values[11] = 800;
	assert(module_monitor_format(line, sizeof(line), "left display", values));
	assert(form_workarea_receive(&state, &snapshot, line) ==
		FORM_WORKAREA_REFUSED);
	/* A valid later sample recovers without a request, poll or new packet. */
	values[11] = 0;
	values[12] = 550;
	values[13] = 0;
	assert(module_monitor_format(line, sizeof(line), "left display", values));
	assert(form_workarea_receive(&state, &snapshot, line) ==
		FORM_WORKAREA_ADMITTED);
	form_workarea_sync(&state, &snapshot);
	assert(form_workarea_bounds(&state, &snapshot, &snapshot.rows[0], &area));
	assert(area.x == 100 && area.y == 600 && area.height == 50);
	available = (form_scale_size){area.width, area.height};
	assert(form_scale_initial_size(800, 8, 6, 50, 30, 6,
		&minimum, &available, &size));
	assert(size.height == minimum.height);
	assert(form_scale_startup_axis(10, size.height + 32,
		area.y, area.height, &position) && position == 600);
	/* Removal/name reuse never reuses an index's reservation. */
	snapshot.epoch++;
	snapshot.count = 1;
	snapshot.rows[0] = snapshot.rows[1];
	form_workarea_sync(&state, &snapshot);
	assert(form_workarea_bounds(&state, &snapshot, &snapshot.rows[0], &area));
	assert(area.x == 0 && area.height == 900);
	snapshot.rows[0].rect.x = INT_MAX;
	assert(!form_workarea_bounds(&state, &snapshot, &snapshot.rows[0], &area));
	assert(area.x == 0 && area.height == 0);
	memset(line, 'x', sizeof(line));
	memcpy(line, "Monitor ", 8);
	line[sizeof(line) - 1] = '\0';
	assert(form_workarea_receive(&state, &snapshot, line) ==
		FORM_WORKAREA_REFUSED);
}

static void test_workarea_limits(void)
{
	module_ui_scale_snapshot snapshot = {.epoch = 1,
		.count = MODULE_UI_SCALE_MAX};
	form_workarea_state state = {0};
	module_monitor_area area;
	module_ui_scale_row *row = &snapshot.rows[MODULE_UI_SCALE_MAX - 1];
	char line[MODULE_UI_SCALE_LINE_MAX + 1];
	int values[MODULE_MONITOR_VALUES] = {0};

	for (unsigned int i = 0; i < snapshot.count; i++)
	{
		snapshot.rows[i] = (module_ui_scale_row){.name_atom = i + 1,
			.quarters = 4, .rect = {0, 0, 800, 600}};
		snprintf(snapshot.rows[i].name, sizeof(snapshot.rows[i].name),
			"display-%u", i);
	}
	/* The longest admitted name can double in size when escaped. */
	memset(row->name, '\'', MODULE_UI_SCALE_NAME_MAX);
	row->name[MODULE_UI_SCALE_NAME_MAX] = '\0';
	values[12] = 40;
	assert(module_monitor_format(line, sizeof(line), row->name, values));
	assert(form_workarea_receive(&state, &snapshot, line) ==
		FORM_WORKAREA_ADMITTED);
	assert(state.valid[MODULE_UI_SCALE_MAX - 1]);
	assert(form_workarea_bounds(&state, &snapshot, row, &area));
	assert(area.y == 40 && area.height == 560);
	/* An unchanged replay retains a sample; an unassociated epoch cannot. */
	form_workarea_sync(&state, &snapshot);
	assert(form_workarea_bounds(&state, &snapshot, row, &area));
	assert(area.y == 40);
	snapshot.epoch++;
	assert(form_workarea_bounds(&state, &snapshot, row, &area));
	assert(area.y == 0 && area.height == 600);
	for (unsigned int i = 10; i < MODULE_MONITOR_VALUES; i++)
	{
		memset(values, 0, sizeof(values));
		values[i] = -1;
		assert(module_monitor_format(line, sizeof(line), row->name, values));
		assert(form_workarea_receive(&state, &snapshot, line) ==
			FORM_WORKAREA_REFUSED);
		assert(!state.valid[MODULE_UI_SCALE_MAX - 1]);
		values[i] = INT_MAX;
		assert(module_monitor_format(line, sizeof(line), row->name, values));
		assert(form_workarea_receive(&state, &snapshot, line) ==
			FORM_WORKAREA_REFUSED);
	}
	assert(form_workarea_receive(&state, &snapshot,
		"Monitor display-0 0 0 0 0 0 0 0 0 0 0 2147483648 0 0 0") ==
		FORM_WORKAREA_REFUSED);
	memset(line, 'x', sizeof(line));
	memcpy(line, "Monitor ", 8);
	line[sizeof(line) - 1] = '\0';
	assert(form_workarea_receive(&state, &snapshot, line) ==
		FORM_WORKAREA_REFUSED);
	snapshot.count++;
	assert(!form_workarea_bounds(&state, &snapshot, row, &area));
	assert(area.width == 0 && area.height == 0);
	assert(form_workarea_receive(&state, &snapshot,
		"Monitor display-0 0 0 0 0 0 0 0 0 0 0 0 0 0 0") ==
		FORM_WORKAREA_REFUSED);
}

static void test_initial_size(void)
{
	form_scale_size minimum = {150, 80}, available = {800, 600}, size;

	assert(form_scale_initial_size(400, 8, 6, 50, 30, 6,
		&minimum, &available, &size));
	assert(size.width == 600 && size.height == 296);
	/* Legal minima remain independent of the preferred dimensions. */
	assert(minimum.width == 150 && minimum.height == 80);
	available = (form_scale_size){200, 100};
	assert(form_scale_initial_size(400, 8, 6, 50, 30, 6,
		&minimum, &available, &size));
	assert(size.width == 200 && size.height == 100);
	available = (form_scale_size){100, 60};
	assert(form_scale_initial_size(400, 8, 6, 50, 30, 6,
		&minimum, &available, &size));
	assert(size.width == 150 && size.height == 80);
	assert(form_scale_initial_size(100, 1, 4, 50, 20, 6,
		&minimum, NULL, &size));
	assert(size.width == 150 && size.height == 80);
	assert(!form_scale_initial_size(INT_MAX, 8, 16, 50, 30, 6,
		&minimum, NULL, &size));
	assert(size.width == 0 && size.height == 0);
	assert(!form_scale_initial_size(400, 33, 6, 50, 30, 6,
		&minimum, NULL, &size));
	assert(!form_scale_initial_size(400, 32, 6, 50, 32767, 6,
		&minimum, NULL, &size));
	assert(!form_scale_initial_size(400, 8, 6, INT_MAX, 30, 6,
		&minimum, NULL, &size));
	minimum.height = 32768;
	assert(!form_scale_initial_size(400, 8, 6, 50, 30, 6,
		&minimum, NULL, &size));
}

static void test_startup_position(void)
{
	int position;

	assert(form_scale_startup_axis(700, 300, 100, 800, &position));
	assert(position == 600);
	assert(form_scale_startup_axis(200, 300, 100, 800, &position));
	assert(position == 200);
	assert(form_scale_startup_axis(-1100, 300, -1000, 800, &position));
	assert(position == -1000);
	assert(form_scale_startup_axis(-300, 300, -1000, 800, &position));
	assert(position == -500);
	/* Oversized frames anchor the readable top/left without retry loops. */
	assert(form_scale_startup_axis(300, 1000, 100, 800, &position));
	assert(position == 100);
	assert(!form_scale_startup_axis(0, 32768, 0, 800, &position));
	assert(!form_scale_startup_axis(0, 300, INT_MAX, 800, &position));
	assert(!form_scale_startup_axis(0, 300, 0, 0, &position));
	assert(position == 0);
}

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
	test_workarea();
	test_workarea_limits();
	test_initial_size();
	test_startup_position();
	test_metrics();
	test_cells();
	test_label_geometry();
	test_label_text();
	test_focus_geometry();
	return 0;
}
