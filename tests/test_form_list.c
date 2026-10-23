/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include <assert.h>
#include <limits.h>
#include "modules/FvwmForm/form_list.h"

int main(void)
{
	form_list_state s = {.rows = 12};
	unsigned char packet[FORM_LIST_BYTES + 2];

	int value;

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
