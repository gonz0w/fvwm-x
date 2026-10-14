/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/script_packet.h"
#include "libs/Module.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void test_text_boundaries(void)
{
	const unsigned long types[] = {
		M_CONFIG_INFO, M_STRING, M_WINDOW_NAME, M_RES_NAME, M_RES_CLASS
	};

	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
	{
		for (size_t words = 0; words <= FvwmPacketBodyMaxSize; words++)
		{
			unsigned long *body = words ? malloc(words * sizeof(*body)) : NULL;
			assert(words == 0 || body != NULL);
			if (body != NULL)
			{
				memset(body, 'x', words * sizeof(*body));
			}
			assert(!script_packet_is_valid(types[i], body, words));
			if (words > 3)
			{
				((char *)body)[words * sizeof(*body) - 1] = '\0';
				assert(script_packet_is_valid(types[i], body, words));
			}
			free(body);
		}
	}
}

static void test_fixed_and_property_bodies(void)
{
	unsigned long body[4] = { MX_PROPERTY_CHANGE_BACKGROUND, 1, 42, 0 };

	assert(script_packet_is_valid(0, NULL, 0));
	assert(!script_packet_is_valid(M_STRING, NULL, 4));
	assert(!script_packet_is_valid(0, body, FvwmPacketBodyMaxSize + 1));
	assert(!script_packet_is_valid(M_MAP, NULL, 0));
	assert(script_packet_is_valid(M_MAP, body, 1));
	for (size_t words = 0; words < 3; words++)
	{
		assert(!script_packet_is_valid(MX_PROPERTY_CHANGE, body, words));
	}
	assert(script_packet_is_valid(MX_PROPERTY_CHANGE, body, 3));
	body[0] = MX_PROPERTY_CHANGE_SWALLOW;
	assert(!script_packet_is_valid(MX_PROPERTY_CHANGE, body, 3));
	assert(script_packet_is_valid(MX_PROPERTY_CHANGE, body, 4));
	body[3] = ~0UL;
	assert(!script_packet_is_valid(MX_PROPERTY_CHANGE, body, 4));
	body[1] = 0;
	assert(script_packet_is_valid(MX_PROPERTY_CHANGE, body, 3));
	body[0] = ~0UL;
	assert(script_packet_is_valid(MX_PROPERTY_CHANGE, body, 3));
}

int main(void)
{
	test_text_boundaries();
	test_fixed_and_property_bodies();
	return 0;
}
