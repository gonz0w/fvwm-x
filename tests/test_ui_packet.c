/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmIconMan/iconman_packet.h"
#include "modules/FvwmButtons/buttons_packet.h"
#include "libs/Module.h"
#include "fvwm/fvwm.h"
#include "libs/vpacket.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef bool (*packet_admission)(unsigned long, const unsigned long *, size_t);

static void test_property_window_id(void)
{
	unsigned long value = 1;
	assert(iconman_parse_window_id("0", &value) && value == 0);
	assert(iconman_parse_window_id("  +123suffix", &value) && value == 123);
	assert(iconman_parse_window_id("4294967295", &value) &&
		value == 4294967295UL);
	const char *bad[] = {"", " ", "x", "-1", "4294967296",
		"18446744073709551616", "999999999999999999999999999999999999"};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		value = 1;
		assert(!iconman_parse_window_id(bad[i], &value));
		assert(value == 0);
	}
	assert(!iconman_parse_window_id(NULL, &value) && value == 0);
	assert(!iconman_parse_window_id("1", NULL));
}

static void test_owner_boundaries(packet_admission admission, bool iconman)
{
	const struct {
		unsigned long type;
		size_t minimum;
	} cases[] = {
		{M_ADD_WINDOW, (sizeof(ConfigWinPacket) + sizeof(unsigned long) - 1) /
			sizeof(unsigned long)},
		{M_CONFIGURE_WINDOW, (sizeof(ConfigWinPacket) + sizeof(unsigned long) - 1) /
			sizeof(unsigned long)},
		{M_NEW_DESK, 1},
		{M_MAP, iconman ? 0 : 1},
		{M_MINI_ICON, iconman ? offsetof(MiniIconPacket, name) /
			sizeof(unsigned long) : 0},
		{M_FOCUS_CHANGE, iconman ? 1 : 2},
		{M_DESTROY_WINDOW, 1},
		{M_WINDOWSHADE, iconman ? 1 : 0},
		{M_DEWINDOWSHADE, iconman ? 1 : 0},
		{M_NEW_PAGE, iconman ? 3 : 0},
		{M_ICONIFY, iconman ? 7 : 0},
		{M_DEICONIFY, iconman ? 7 : 0},
		{M_ICON_LOCATION, iconman ? 7 : 0}
	};
	const unsigned long text[] = {M_RES_NAME, M_RES_CLASS, M_WINDOW_NAME,
		M_CONFIG_INFO, M_STRING};
	const unsigned long icon_text[] = {M_ICON_NAME, M_VISIBLE_NAME,
		MX_VISIBLE_ICON_NAME};

	for (size_t words = 0; words <= FvwmPacketBodyMaxSize; words++)
	{
		unsigned long *body = words ? calloc(words, sizeof(*body)) : NULL;

		assert(words == 0 || body != NULL);
		for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
		{
			assert(admission(cases[i].type, body, words) ==
				(words >= cases[i].minimum));
		}
		assert(admission(MX_PROPERTY_CHANGE, body, words) == (words >= 3));
		if (words >= 2)
		{
			body[0] = MX_PROPERTY_CHANGE_SWALLOW;
			body[1] = 1;
		}
		assert(admission(MX_PROPERTY_CHANGE, body, words) == (words > 3));
		if (body != NULL)
		{
			memset(body, 'x', words * sizeof(*body));
		}
		for (size_t i = 0; i < sizeof(text) / sizeof(text[0]); i++)
		{
			assert(!admission(text[i], body, words));
		}
		for (size_t i = 0; i < sizeof(icon_text) / sizeof(icon_text[0]); i++)
		{
			assert(admission(icon_text[i], body, words) == !iconman);
		}
		if (words > 3)
		{
			((char *)body)[words * sizeof(*body) - 1] = '\0';
			for (size_t i = 0; i < sizeof(text) / sizeof(text[0]); i++)
			{
				assert(admission(text[i], body, words));
			}
			for (size_t i = 0; i < sizeof(icon_text) / sizeof(icon_text[0]); i++)
			{
				assert(admission(icon_text[i], body, words));
			}
		}
		free(body);
	}
	assert(admission(M_END_WINDOWLIST, NULL, 0));
	assert(!admission(0, NULL, 1));
	unsigned long word = 0;
	assert(!admission(0, &word, FvwmPacketBodyMaxSize + 1));
}

int main(void)
{
	test_property_window_id();
	test_owner_boundaries(iconman_packet_is_valid, true);
	test_owner_boundaries(buttons_packet_is_valid, false);
	return 0;
}
