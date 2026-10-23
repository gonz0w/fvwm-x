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

static void test_updates(void)
{
	form_list_state s = {.rows = 3, .generation = 1};
	form_list_state before;
	form_list_update u, saved;
	unsigned char packet[FORM_LIST_PACKET_MAX];

	for (int i = 0; i < 20; i++) assert(form_list_add(&s, i == 18));
	form_list_focus(&s, 18);
	assert(s.top == 16);
	assert(!form_list_update_begin(&u, &s, 0, 2));
	assert(!form_list_update_begin(&u, &s, 1, 1));
	assert(form_list_update_begin(&u, &s, 1, 2));
	assert(form_list_update_add(&u, 0));
	for (int i = 2; i <= 20; i++) assert(form_list_update_add(&u, i));
	/* User input during staging wins over the state at Begin. */
	assert(form_list_toggle(&s, 5));
	form_list_focus(&s, 5);
	form_list_scroll(&s, INT_MAX);
	assert(form_list_update_commit(&s, &u));
	assert(s.generation == 2 && s.selected == 2 && s.focus == 5);
	assert(s.bits[0] == 32 && s.bits[2] == 4 && s.top == 17);
	assert(form_list_packet(&s, packet) == 9);
	assert(packet[0] == 0 && packet[3] == 2 && packet[5] == 5);
	before = s;
	assert(!form_list_update_commit(&s, &u));
	assert(memcmp(&s, &before, sizeof(s)) == 0);
	assert(form_list_update_begin(&u, &s, 2, 3));
	assert(form_list_update_add(&u, 1));
	assert(!form_list_update_add(&u, 1));
	assert(!form_list_update_commit(&s, &u));
	assert(memcmp(&s, &before, sizeof(s)) == 0);
	assert(form_list_update_begin(&u, &s, 2, 3));
	assert(!form_list_update_add(&u, 21));
	assert(!form_list_update_add(&u, 0));
	assert(!form_list_update_commit(&s, &u));
	assert(form_list_update_begin(&u, &s, 2, 3));
	for (int i = 0; i < FORM_LIST_MAX; i++)
		assert(form_list_update_add(&u, 0));
	saved = u;
	assert(!form_list_update_add(&u, 0));
	assert(!form_list_update_commit(&s, &u));
	assert(form_list_update_commit(&s, &saved));
	assert(s.count == FORM_LIST_MAX && s.selected == 0 && s.focus == 0);
	assert(form_list_update_begin(&u, &s, 3, INT_MAX));
	/* An empty snapshot is an atomic clear, including stale bitmap bytes. */
	assert(form_list_update_commit(&s, &u));
	assert(s.count == 0 && s.focus == 0 && s.top == 0 && s.selected == 0);
	assert(form_list_packet(&s, packet) == 6);
	assert(packet[0] == 127 && packet[1] == 255 && packet[3] == 255);
	assert(!form_list_update_begin(&u, &s, INT_MAX, INT_MAX));

	s = (form_list_state){.rows = 2, .columns = 4, .generation = 1};
	for (int i = 0; i < 20; i++) assert(form_list_add(&s, i == 10));
	form_list_focus(&s, 10);
	form_list_scroll(&s, INT_MAX);
	assert(form_list_update_begin(&u, &s, 1, 2));
	for (int i = 20; i > 0; i--)
		if (i != 11) assert(form_list_update_add(&u, i));
	assert(form_list_update_commit(&s, &u));
	/* Removed focus chooses old successor 12 (new zero-based index 8). */
	assert(s.focus == 8 && s.anchor == 8 && s.selected == 0);
	assert(s.top % 4 == 0 && s.top <= 12);
	s.generation = 0;
	assert(!form_list_update_begin(&u, &s, 0, 1));
}

static void test_live_choices_and_status(void)
{
	form_list_state s = {.rows = 12, .generation = 1};
	form_list_state before = s;
	form_list_update u;
	char token[65];
	bool selection;

	assert(form_list_status_token("owner_2-u selection", token, &selection));
	assert(selection && strcmp(token, "owner_2-u") == 0);
	assert(form_list_status_token("owner ", token, &selection) && !selection);
	const char *bad[] = {"", "has space", "x;echo", "x selection extra", "x selectionx"};
	for (int i = 0; i < 5; i++)
		assert(!form_list_status_token(bad[i], token, &selection) && token[0] == '\0');
	assert(form_list_update_begin(&u, &s, 1, 2));
	for (int i = 0; i < 257; i++)
		assert(form_list_update_add_choice(&u, 0, FORM_LIST_ON));
	assert(!form_list_update_commit(&s, &u));
	assert(memcmp(&s, &before, sizeof(s)) == 0);
	u.choices[256] = FORM_LIST_OFF;
	assert(form_list_update_commit(&s, &u) && s.selected == 256);
	assert(form_list_update_begin(&u, &s, 2, 3));
	assert(form_list_update_add_choice(&u, 1, FORM_LIST_OFF));
	assert(form_list_update_add_choice(&u, 257, FORM_LIST_ON));
	assert(form_list_update_commit(&s, &u));
	assert(s.selected == 1 && s.bits[0] == 2);
	assert(form_list_update_begin(&u, &s, 3, 4));
	assert(!form_list_update_add_choice(&u, 0, 42));
	assert(!form_list_update_commit(&s, &u));
}

int main(void)
{
	test_grid_ranges();
	test_updates();
	test_live_choices_and_status();
	form_list_state s = {.rows = 12};
	unsigned char packet[FORM_LIST_PACKET_MAX];

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
