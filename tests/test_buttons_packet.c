/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "modules/FvwmButtons/buttons_packet.h"
#include "libs/Module.h"
#include <assert.h>

int main(void)
{
	unsigned long words[3] = {10, 20, 0};
	assert(!buttons_packet_is_valid(M_FOCUS_CHANGE, NULL, 0));
	assert(!buttons_packet_is_valid(M_DESTROY_WINDOW, NULL, 0));
	assert(!buttons_packet_is_valid(M_FOCUS_CHANGE, words, 1));
	assert(buttons_packet_is_valid(M_FOCUS_CHANGE, words, 2));
	assert(buttons_packet_is_valid(M_DESTROY_WINDOW, words, 1));
	assert(buttons_packet_is_valid(M_FOCUS_CHANGE, words, 3));
	return 0;
}
