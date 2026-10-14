/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmPager/pager_packet.h"
#include "libs/Module.h"
#include "fvwm/fvwm.h"
#include "libs/vpacket.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void test_exact_body_boundaries(void)
{
	const struct {
		unsigned long type;
		size_t minimum;
	} cases[] = {
		{M_ADD_WINDOW, (sizeof(ConfigWinPacket) + sizeof(unsigned long) - 1) /
			sizeof(unsigned long)},
		{M_CONFIGURE_WINDOW,
			(sizeof(ConfigWinPacket) + sizeof(unsigned long) - 1) /
				sizeof(unsigned long)},
		{M_MINI_ICON, offsetof(MiniIconPacket, name) / sizeof(unsigned long)},
		{M_DESTROY_WINDOW, 1}, {M_RAISE_WINDOW, 1}, {M_LOWER_WINDOW, 1},
		{M_DEICONIFY, 1}, {M_NEW_DESK, 2}, {MX_PROPERTY_CHANGE, 3},
		{M_FOCUS_CHANGE, 5}, {M_ICONIFY, 7}, {M_ICON_LOCATION, 7},
		{M_NEW_PAGE, 8}
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		for (size_t words = 0; words <= FvwmPacketBodyMaxSize; words++)
		{
			unsigned long *body = words ? calloc(words, sizeof(*body)) : NULL;

			assert(words == 0 || body != NULL);
			assert(pager_packet_is_valid(cases[i].type, body, words) ==
				(words >= cases[i].minimum));
			free(body);
		}
	}
}

static void test_text_and_restack_boundaries(void)
{
	const unsigned long types[] = {M_VISIBLE_NAME, M_RES_NAME, M_RES_CLASS,
		MX_VISIBLE_ICON_NAME, M_CONFIG_INFO, M_STRING, MX_REPLY};

	for (size_t words = 0; words <= FvwmPacketBodyMaxSize; words++)
	{
		unsigned long *body = words ? malloc(words * sizeof(*body)) : NULL;

		assert(words == 0 || body != NULL);
		if (body != NULL)
		{
			memset(body, 'x', words * sizeof(*body));
		}
		assert(pager_packet_is_valid(M_RESTACK, body, words) == (words % 3 == 0));
		for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
		{
			assert(!pager_packet_is_valid(types[i], body, words));
		}
		if (words > 3)
		{
			((char *)body)[words * sizeof(*body) - 1] = '\0';
			for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
			{
				assert(pager_packet_is_valid(types[i], body, words));
			}
		}
		free(body);
	}
	assert(pager_packet_is_valid(0, NULL, 0));
	assert(pager_packet_is_valid(MX_MONITOR_FOCUS, NULL, 0));
	assert(!pager_packet_is_valid(0, NULL, 1));
	unsigned long body = 0;
	assert(!pager_packet_is_valid(0, &body, FvwmPacketBodyMaxSize + 1));
}

int main(void)
{
	test_exact_body_boundaries();
	test_text_and_restack_boundaries();
	return 0;
}
