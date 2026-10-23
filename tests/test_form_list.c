/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
#include "modules/FvwmForm/form_list.h"

static void test_grid_ranges(void)
{
	form_list_state s = {.rows = 4, .columns = 4};
	form_list_state before;

	for (int i = 0; i < 19; i++) assert(form_list_add(&s, false));
	assert(form_list_lines(&s) == 5);
	form_list_focus(&s, 18);
	assert(s.focus == 18 && s.top == 4 && s.anchor == 18);
	assert(form_list_extend(&s, 14));
	assert(s.focus == 14 && s.anchor == 18 && s.selected == 5);
	assert(form_list_extend(&s, 17) && s.selected == 5);
	form_list_scroll(&s, INT_MIN);
	assert(s.top == 0);
	form_list_scroll(&s, INT_MAX);
	assert(s.top == 4);
	form_list_focus(&s, 0);
	assert(s.top == 0 && s.anchor == 0);
	for (int i = 19; i < FORM_LIST_MAX; i++) assert(form_list_add(&s, false));
	form_list_focus(&s, 4095);
	assert(s.top == 4080 && s.top % 4 == 0);
	before = s;
	assert(!form_list_extend(&s, 0));
	assert(s.focus == before.focus && s.anchor == before.anchor &&
		s.top == before.top && s.selected == before.selected &&
		memcmp(s.bits, before.bits, sizeof(s.bits)) == 0);
	assert(form_list_extend(&s, 4090) && s.selected == 11);
	s = (form_list_state){.rows = 4, .columns = 4};
	form_list_focus(&s, INT_MAX);
	assert(s.top == 0 && s.focus == 0 && !form_list_extend(&s, 0));
	assert(form_list_add(&s, false));
	form_list_focus(&s, 0);
	assert(form_list_extend(&s, 0) && s.selected == 1);
}

int main(void)
{
	test_grid_ranges();
	form_list_state s = {.rows = 12};
	unsigned char packet[FORM_LIST_BYTES + 2];

	int value;
	enum form_list_icon icon;

	assert(form_list_parse_icon("directory ", &icon) &&
		icon == FORM_ICON_DIRECTORY);
	assert(form_list_parse_icon("", &icon) && icon == FORM_ICON_NONE);
	assert(!form_list_parse_icon("file extra", &icon));
	assert(!form_list_parse_icon("/path/icon.png", &icon));

	assert(form_list_number("32", 32, &value) && value == 32);
	assert(form_list_number("12 ", 32, &value) && value == 12);
	for (int i = 0; i < 6; i++)
	{
		const char *invalid[] = {"0", "-1", "33", "12x", "", "999999999999"};
		assert(!form_list_number(invalid[i], 32, &value) && value == 0);
	}
	assert(form_list_packet(&s, packet) == 2);
	assert(!form_list_toggle(&s, 0));
	for (int i = 0; i < FORM_LIST_MAX; i++)
		assert(form_list_add(&s, i < FORM_LIST_SELECTED));
	assert(!form_list_toggle(&s, FORM_LIST_SELECTED));
	assert(form_list_toggle(&s, 0));
	assert(form_list_toggle(&s, FORM_LIST_MAX - 1));
	assert(!form_list_toggle(&s, -1));
	assert(!form_list_toggle(&s, FORM_LIST_MAX));
	form_list_focus(&s, INT_MAX);
	assert(s.focus == 4095 && s.top == 4084);
	form_list_scroll(&s, INT_MIN);
	assert(s.top == 0 && s.focus == 4095);
	form_list_scroll(&s, INT_MAX);
	assert(s.top == 4084);
	assert(form_list_packet(&s, packet) == 514);
	assert(packet[0] == 15 && packet[1] == 255);
	assert(packet[2] == 254 && packet[513] == 128);
	form_list_focus(&s, -1);
	assert(s.top == 0 && s.focus == 0);
	assert(!form_list_add(&s, false));
	assert(form_list_packet(&s, packet) == 0);
	assert(!form_list_toggle(&s, 1));
	return 0;
}
