/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "modules/FvwmMFL/event_flags.h"
#include <assert.h>

static void test_extended_flags_keep_wire_width(void)
{
	assert(fvwmmfl_event_flag_matches(MX_ECHO, MX_ECHO));
	assert(fvwmmfl_event_flag_matches(MX_ENTER_WINDOW, MX_ENTER_WINDOW));
	assert(!fvwmmfl_event_flag_matches(MX_ECHO, MX_ENTER_WINDOW));
	assert(!fvwmmfl_event_flag_matches(MX_ECHO, M_DEICONIFY));
	assert(!fvwmmfl_event_flag_matches(M_DEICONIFY, MX_ECHO));
}

static void test_standard_masks_and_unknown_types(void)
{
	assert(fvwmmfl_event_flag_matches(M_ADD_WINDOW, M_ADD_WINDOW));
	assert(fvwmmfl_event_flag_matches(
	    M_ADD_WINDOW | M_CONFIGURE_WINDOW, M_ADD_WINDOW));
	assert(!fvwmmfl_event_flag_matches(M_ADD_WINDOW, M_DESTROY_WINDOW));
	assert(!fvwmmfl_event_flag_matches(0, M_ADD_WINDOW));
	assert(!fvwmmfl_event_flag_matches(
	    M_EXTENDED_MSG | (1UL << 30), MX_ECHO));
}

static void test_unsubscribe_preserves_other_extended_events(void)
{
	unsigned long mask = MX_ECHO | MX_ENTER_WINDOW;
	mask = fvwmmfl_event_flag_remove(mask, MX_ENTER_WINDOW);
	assert((mask & MX_ECHO) == MX_ECHO);
	assert((mask & (MX_ENTER_WINDOW & ~M_EXTENDED_MSG)) == 0);
	mask = fvwmmfl_event_flag_remove(mask, MX_ECHO);
	assert((mask & (MX_ECHO & ~M_EXTENDED_MSG)) == 0);
}

int main(void)
{
	test_unsubscribe_preserves_other_extended_events();
	test_extended_flags_keep_wire_width();
	test_standard_masks_and_unknown_types();
	return 0;
}
