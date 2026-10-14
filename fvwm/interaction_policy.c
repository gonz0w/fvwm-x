/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include "interaction.h"
#include "libs/defaults.h"

unsigned int fvwm_interaction_button_mask(unsigned int button)
{
	if (button == 0 || button > NUMBER_OF_MOUSE_BUTTONS)
	{
		return 0;
	}
	return (unsigned int)Button1Mask << (button - 1);
}

unsigned int fvwm_button_release_mask(
	unsigned int previous, unsigned int button, unsigned int event_state)
{
	unsigned int mask = fvwm_interaction_button_mask(button);

	if (mask == 0)
	{
		return previous & DEFAULT_ALL_BUTTONS_MASK;
	}
	return event_state & DEFAULT_ALL_BUTTONS_MASK & ~mask;
}

int fvwm_button_release_timeout(uint64_t previous_ns, uint64_t now_ns)
{
	uint64_t elapsed_ns;
	uint64_t remaining_ns;

	if (now_ns == UINT64_MAX || previous_ns == UINT64_MAX ||
	    now_ns < previous_ns)
	{
		return FVWM_BUTTON_RELEASE_INTERVAL_MS;
	}
	elapsed_ns = now_ns - previous_ns;
	if (elapsed_ns >= FVWM_BUTTON_RELEASE_INTERVAL_NS)
	{
		return 0;
	}
	remaining_ns = FVWM_BUTTON_RELEASE_INTERVAL_NS - elapsed_ns;
	return (int)((remaining_ns + UINT64_C(999999)) / UINT64_C(1000000));
}

Bool fvwm_interaction_target_event(
	const fvwm_interaction *interaction, const XEvent *event)
{
	if (interaction->client == None)
	{
		return False;
	}
	if (event->type == DestroyNotify)
	{
		return event->xdestroywindow.window == interaction->client;
	}
	if (event->type == UnmapNotify && interaction->can_withdraw)
	{
		/* Match HandleUnmapNotify's withdrawal rule; parent copies and
		 * frame/icon unmaps are not client withdrawals. */
		return event->xunmap.window == interaction->client &&
			(event->xunmap.event == interaction->client ||
			 (event->xunmap.event == interaction->root &&
			  event->xunmap.send_event));
	}

	return False;
}
