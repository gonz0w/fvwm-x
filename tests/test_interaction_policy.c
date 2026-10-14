/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include <assert.h>
#include "fvwm/interaction.h"
#include "libs/defaults.h"

static void test_button_masks(void)
{
	assert(fvwm_interaction_button_mask(0) == 0);
	assert(fvwm_interaction_button_mask(1) == Button1Mask);
	assert(fvwm_interaction_button_mask(2) == Button2Mask);
	assert(fvwm_interaction_button_mask(5) == Button5Mask);
	assert(fvwm_interaction_button_mask(6) == 0);
	assert(fvwm_interaction_button_mask(UINT32_MAX) == 0);
}

static void test_button_release_state(void)
{
	unsigned int buttons = Button1Mask | Button5Mask;

	assert(fvwm_button_release_mask(buttons, 1,
		buttons | ShiftMask | Mod4Mask) == Button5Mask);
	assert(fvwm_button_release_mask(buttons, 5, buttons) == Button1Mask);
	assert(fvwm_button_release_mask(Button1Mask, 1,
		Button1Mask | ControlMask) == 0);
	assert(fvwm_button_release_mask(buttons | ShiftMask, 0, 0) == buttons);
	assert(fvwm_button_release_mask(buttons, 6, 0) == buttons);
	assert(fvwm_button_release_mask(buttons, UINT32_MAX, 0) == buttons);
}

static void test_button_release_deadlines(void)
{
	uint64_t start = UINT64_C(123456789);

	assert(fvwm_button_release_timeout(start, start) == 20);
	assert(fvwm_button_release_timeout(start, start + 1) == 20);
	assert(fvwm_button_release_timeout(start,
		start + UINT64_C(1000000)) == 19);
	assert(fvwm_button_release_timeout(start,
		start + FVWM_BUTTON_RELEASE_INTERVAL_NS - 1) == 1);
	assert(fvwm_button_release_timeout(start,
		start + FVWM_BUTTON_RELEASE_INTERVAL_NS) == 0);
	assert(fvwm_button_release_timeout(start, UINT64_MAX - 1) == 0);
	assert(fvwm_button_release_timeout(start, start - 1) == 20);
	assert(fvwm_button_release_timeout(start, UINT64_MAX) == 20);
	assert(fvwm_button_release_timeout(UINT64_MAX, start) == 20);
	assert(fvwm_button_release_timeout(UINT64_MAX - 10,
		UINT64_MAX - 1) == 20);
}

static void test_target_destruction(void)
{
	fvwm_interaction interaction = { 42, 1, True,
		FVWM_INTERACTION_RUNNING, NULL };
	XEvent event = { 0 };

	event.type = DestroyNotify;
	event.xdestroywindow.window = 43;
	assert(!fvwm_interaction_target_event(&interaction, &event));
	event.xdestroywindow.window = 42;
	assert(fvwm_interaction_target_event(&interaction, &event));
	interaction.client = None;
	event.xdestroywindow.window = None;
	assert(!fvwm_interaction_target_event(&interaction, &event));
}

static void test_withdrawal_copies_and_synthetic_events(void)
{
	fvwm_interaction interaction = { 42, 1, True,
		FVWM_INTERACTION_RUNNING, NULL };
	XEvent event = { 0 };

	event.type = UnmapNotify;
	event.xunmap.window = 42;
	event.xunmap.event = 9;
	assert(!fvwm_interaction_target_event(&interaction, &event));
	event.xunmap.event = 1;
	assert(!fvwm_interaction_target_event(&interaction, &event));
	event.xunmap.send_event = True;
	assert(fvwm_interaction_target_event(&interaction, &event));
	event.xunmap.event = 9;
	assert(!fvwm_interaction_target_event(&interaction, &event));
	event.xunmap.event = 42;
	assert(fvwm_interaction_target_event(&interaction, &event));
	event.xunmap.send_event = False;
	assert(fvwm_interaction_target_event(&interaction, &event));
	event.xunmap.window = 43;
	assert(!fvwm_interaction_target_event(&interaction, &event));
	interaction.can_withdraw = False;
	event.xunmap.window = 42;
	assert(!fvwm_interaction_target_event(&interaction, &event));
	event.type = MotionNotify;
	assert(!fvwm_interaction_target_event(&interaction, &event));
}

int main(void)
{
	test_target_destruction();
	test_withdrawal_copies_and_synthetic_events();
	test_button_masks();
	test_button_release_state();
	test_button_release_deadlines();
	return 0;
}
