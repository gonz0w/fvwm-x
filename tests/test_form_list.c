/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "modules/FvwmForm/form_list.h"

static void test_viewport(void)
{
	form_list_state s = {.rows = 4, .columns = 4, .generation = 7};
	form_list_state before;
	unsigned char packet[FORM_LIST_PACKET_MAX], after[FORM_LIST_PACKET_MAX];
	size_t length;

	for (int i = 0; i < 100; i++)
		assert(form_list_add(&s, i == 12 || i == 99));
	form_list_focus(&s, 45);
	assert(form_list_extend(&s, 47));
	form_list_scroll(&s, -3);
	before = s;
	length = form_list_packet(&s, packet);
	assert(form_list_viewport(&s, 6, 3));
	assert(s.top == before.top / 3 * 3 && s.rows == 6 && s.columns == 3);
	assert(s.focus == before.focus && s.anchor == before.anchor);
	assert(form_list_packet(&s, after) == length);
	assert(memcmp(packet, after, length) == 0);
	before = s;
	assert(!form_list_viewport(&s, 0, 3));
	assert(!form_list_viewport(&s, 33, 3));
	assert(!form_list_viewport(&s, 3, 0));
	assert(!form_list_viewport(&s, 3, 9));
	assert(memcmp(&s, &before, sizeof(s)) == 0);
	form_list_scroll(&s, INT_MAX);
	assert(form_list_viewport(&s, 32, 8) && s.top == 0);
	assert(s.focus == before.focus && s.anchor == before.anchor);
	s = (form_list_state){.rows = 1};
	assert(form_list_viewport(&s, 32, 8));
	assert(s.top == 0 && s.count == 0 && s.generation == 0);
	assert(!form_list_viewport(NULL, 1, 1));
}

static void test_versioned_focus(void)
{
	form_list_state s = {.rows = 4, .columns = 4, .generation = 7};
	form_list_state before;

	for (int i = 0; i < FORM_LIST_MAX; i++)
		assert(form_list_add(&s, i == 0));
	before = s;
	assert(!form_list_focus_version(&s, 0, 0));
	assert(!form_list_focus_version(&s, 6, 4095));
	assert(!form_list_focus_version(&s, 7, -1));
	assert(!form_list_focus_version(&s, 7, 4096));
	assert(memcmp(&s, &before, sizeof(s)) == 0);
	assert(form_list_focus_version(&s, 7, 4095));
	assert(s.focus == 4095 && s.anchor == 4095 && s.top == 4080);
	assert(s.selected == before.selected &&
		memcmp(s.bits, before.bits, sizeof(s.bits)) == 0);
	s.refused = true;
	before = s;
	assert(!form_list_focus_version(&s, 7, 0));
	assert(memcmp(&s, &before, sizeof(s)) == 0);
	s = (form_list_state){.generation = 7};
	assert(!form_list_focus_version(&s, 7, 0));
}

static void test_live_layout(void)
{
	form_list_state s = {.rows = 4, .columns = 1, .generation = 7};
	form_list_state before, expected;
	unsigned char packet[FORM_LIST_PACKET_MAX];

	for (int i = 0; i < 100; i++)
		assert(form_list_add(&s, i == 12 || i == 99));
	form_list_focus(&s, 45);
	assert(form_list_extend(&s, 47));
	form_list_scroll(&s, INT_MIN);
	before = s;
	assert(form_list_layout(&s, 7, 8, 6, 3));
	expected = before;
	expected.rows = 6;
	expected.columns = 3;
	expected.generation = 8;
	assert(memcmp(&s, &expected, sizeof(s)) == 0);
	/* A wheel-scrolled focus stays offscreen; its range anchor survives. */
	assert(s.focus == 47 && s.anchor == 45 && s.top == 0);
	assert(form_list_packet(&s, packet) == 19 && packet[3] == 8);
	assert(!form_list_focus_version(&s, 7, 0));
	form_list_scroll(&s, 5);
	assert(s.top == 15);
	assert(form_list_layout(&s, 8, 9, 4, 1) && s.top == 15);
	assert(form_list_layout(&s, 9, 10, 4, 8) && s.top == 8);
	assert(s.focus == 47 && s.anchor == 45 &&
		memcmp(s.bits, before.bits, sizeof(s.bits)) == 0);
	form_list_scroll(&s, INT_MAX);
	assert(form_list_layout(&s, 10, 11, 32, 8) && s.top == 0);
	before = s;
	assert(!form_list_layout(NULL, 11, 12, 1, 1));
	assert(!form_list_layout(&s, 0, 12, 1, 1));
	assert(!form_list_layout(&s, 10, 12, 1, 1));
	assert(!form_list_layout(&s, 11, 11, 1, 1));
	assert(!form_list_layout(&s, 11, 10, 1, 1));
	assert(!form_list_layout(&s, 11, 12, 0, 1));
	assert(!form_list_layout(&s, 11, 12, 33, 1));
	assert(!form_list_layout(&s, 11, 12, 1, 0));
	assert(!form_list_layout(&s, 11, 12, 1, 9));
	assert(memcmp(&s, &before, sizeof(s)) == 0);
	s.refused = true;
	before = s;
	assert(!form_list_layout(&s, 11, 12, 1, 1));
	assert(memcmp(&s, &before, sizeof(s)) == 0);
	s = (form_list_state){.rows = 1};
	assert(!form_list_layout(&s, 0, 1, 1, 1));
	s.generation = 1;
	assert(form_list_layout(&s, 1, INT_MAX, 32, 8));
	assert(s.count == 0 && s.top == 0 && s.generation == INT_MAX);
	before = s;
	assert(!form_list_layout(&s, INT_MAX, INT_MAX, 1, 1));
	assert(memcmp(&s, &before, sizeof(s)) == 0);
}

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

static void test_watch_validation(void)
{
	form_list_state s = {.rows = 3};
	form_list_watch watch = {0}, saved;
	char request[65], longest[65], oversized[66], command[140];
	const char *bad[] = {"", " token", "token selection", "has space",
		"x;echo", "x/y", "x\xc3\xa9", "x selection extra"};

	assert(!form_list_watch_begin(&watch, &s, "a"));
	assert(watch.token[0] == '\0' && !watch.outstanding);
	s.generation = 1;
	assert(form_list_watch_begin(&watch, &s, "a"));
	assert(strcmp(watch.token, "a") == 0 && !watch.outstanding);
	memset(longest, 'Z', 64);
	longest[64] = '\0';
	assert(form_list_watch_begin(&watch, &s, longest));
	assert(strcmp(watch.token, longest) == 0);
	memset(oversized, 'Z', 65);
	oversized[65] = '\0';
	watch.outstanding = true;
	saved = watch;
	assert(!form_list_watch_begin(&watch, &s, oversized));
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
		assert(!form_list_watch_begin(&watch, &s, bad[i]));
	assert(strcmp(watch.token, saved.token) == 0 && watch.outstanding);
	s.refused = true;
	assert(!form_list_watch_begin(&watch, &s, "replacement"));
	s.refused = false;
	assert(form_list_watch_begin(&watch, &s, "owner_2-u"));
	watch.outstanding = true;
	const char *bad_requests[] = {"", "owner_2-u", "wrong request",
		"owner_2-u request selection", "owner_2-u request extra",
		"owner_2-u has;command", "owner_2-u x\xc3\xa9"};
	for (size_t i = 0;
	     i < sizeof(bad_requests) / sizeof(bad_requests[0]); i++)
	{
		assert(!form_list_watch_request(&watch, &s, bad_requests[i],
			request));
		assert(request[0] == '\0' && watch.outstanding);
	}
	snprintf(command, sizeof(command), "owner_2-u %s", oversized);
	assert(!form_list_watch_request(&watch, &s, command, request));
	snprintf(command, sizeof(command), "owner_2-u %s  ", longest);
	assert(form_list_watch_request(&watch, &s, command, request));
	assert(strcmp(request, longest) == 0 && watch.outstanding);
	s.refused = true;
	assert(!form_list_watch_request(&watch, &s, command, request));
	assert(request[0] == '\0' && watch.outstanding);
	s.refused = false;
	s.generation = 0;
	assert(!form_list_watch_request(&watch, &s, command, request));
	s.generation = 1;
	watch = (form_list_watch){0};
	assert(!form_list_watch_request(&watch, &s, command, request));
}

static void test_watch_mutations(void)
{
	form_list_state s = {.rows = 3, .generation = 1}, before;
	form_list_watch watch = {0};
	form_list_update update;
	char request[65];
	bool selection;
	unsigned char packet[FORM_LIST_PACKET_MAX];

	for (int i = 0; i < 20; i++)
		assert(form_list_add(&s, i == 0));
	before = s;
	form_list_focus(&s, 1);
	assert(!form_list_watch_changed(&watch, &before, &s));
	assert(form_list_watch_begin(&watch, &s, "watch"));
	/* Ignore anchor/layout/generation; observe interactive content. */
	before = s;
	before.anchor = 12;
	before.rows = 10;
	before.columns = 2;
	before.generation = 42;
	assert(!form_list_watch_changed(&watch, &before, &s));
	before = s;
	form_list_focus(&s, 1);
	form_list_scroll(&s, INT_MIN);
	assert(!form_list_watch_changed(&watch, &before, &s));
	before = s;
	form_list_focus(&s, 2);
	assert(form_list_watch_changed(&watch, &before, &s));
	before = s;
	assert(form_list_toggle(&s, 3));
	assert(!form_list_watch_changed(&watch, &before, &s));
	/* Ordinary status and validation cannot release the marker. */
	assert(form_list_status_token("probe selection", request, &selection));
	assert(selection && watch.outstanding);
	assert(form_list_watch_request(&watch, &s, "watch latest", request));
	assert(watch.outstanding && strcmp(request, "latest") == 0);
	assert(form_list_packet(&s, packet) == 9);
	assert(packet[3] == 1 && packet[5] == 2 && packet[6] == 9);
	/* Publication may remap every ordinal while the marker is latched. */
	assert(form_list_update_begin(&update, &s, 1, 2));
	for (int i = 20; i > 0; i--)
		assert(form_list_update_add(&update, i));
	assert(form_list_update_commit(&s, &update));
	assert(form_list_watch_request(&watch, &s, "watch remapped", request));
	assert(form_list_packet(&s, packet) == 9);
	assert(packet[3] == 2 && packet[5] == 17 && s.selected == 2);
	assert(s.bits[2] == 9 && watch.outstanding);
	/* The native handler clears only after it sends this current report. */
	watch.outstanding = false;
	before = s;
	form_list_scroll(&s, INT_MIN);
	assert(form_list_watch_changed(&watch, &before, &s));
	watch.outstanding = false;
	before = s;
	form_list_scroll(&s, INT_MIN);
	assert(!form_list_watch_changed(&watch, &before, &s));
	before = s;
	assert(form_list_toggle(&s, 16));
	assert(form_list_toggle(&s, 15));
	assert(s.selected == before.selected);
	assert(form_list_watch_changed(&watch, &before, &s));
	watch.outstanding = false;
	before = s;
	assert(form_list_toggle(&s, 1));
	assert(form_list_watch_changed(&watch, &before, &s));
	/* Empty/clamped input and refused ranges publish no mutation. */
	s = (form_list_state){.rows = 3, .generation = 2};
	watch.outstanding = false;
	before = s;
	form_list_focus(&s, INT_MAX);
	assert(!form_list_toggle(&s, 0));
	assert(!form_list_extend(&s, 0));
	form_list_scroll(&s, INT_MAX);
	assert(!form_list_watch_changed(&watch, &before, &s));
	for (int i = 0; i < 300; i++)
		assert(form_list_add(&s, i < FORM_LIST_SELECTED));
	before = s;
	assert(!form_list_extend(&s, 299));
	assert(!form_list_toggle(&s, 299));
	assert(!form_list_watch_changed(&watch, &before, &s));
}

int main(void)
{
	test_watch_validation();
	test_watch_mutations();
	test_viewport();
	test_grid_ranges();
	test_versioned_focus();
	test_live_layout();
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
