/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include <assert.h>
#include "fvwm/interaction.h"
#include "libs/FEvent.h"

Display *dpy;

static void test_service_event_cache_restore(void)
{
	XEvent event = { 0 };
	void *saved;

	event.type = MotionNotify;
	event.xmotion.x_root = 11;
	fev_fake_event(&event);
	event.xmotion.x_root = 22;
	fev_fake_event(&event);
	saved = fev_save_event();
	event.xmotion.x_root = 33;
	fev_fake_event(&event);
	fev_restore_event(saved);
	fev_get_last_event(&event);
	assert(event.xmotion.x_root == 22);
	FPutBackEvent(dpy, &event);
	fev_get_last_event(&event);
	assert(event.xmotion.x_root == 11);
	FNextEvent(dpy, &event);
	assert(event.xmotion.x_root == 22);
}

static void test_bounded_motion_and_release(void)
{
	XEvent event = { 0 };
	int index;
	const long mask = PointerMotionMask | ButtonReleaseMask | KeyPressMask;

	/* Queue more motion than two turns can consume, followed by a
	 * release and a later motion. The release must remain a barrier. */
	for (index = 72; index >= 1; index--)
	{
		event.type = index == 71 ? ButtonRelease : MotionNotify;
		event.xmotion.x_root = index;
		FPutBackEvent(dpy, &event);
	}
	fvwm_interaction_coalesce(mask, &event);
	assert(event.type == MotionNotify);
	assert(event.xmotion.x_root == FVWM_INTERACTION_MOTION_BUDGET);
	fvwm_interaction_coalesce(mask, &event);
	assert(event.xmotion.x_root == 2 * FVWM_INTERACTION_MOTION_BUDGET);
	fvwm_interaction_coalesce(mask, &event);
	assert(event.type == ButtonRelease);
	assert(event.xbutton.x_root == 71);
	FNextEvent(dpy, &event);
	assert(event.type == MotionNotify && event.xmotion.x_root == 72);

	event.type = MotionNotify;
	FPutBackEvent(dpy, &event);
	event.type = KeyPress;
	FPutBackEvent(dpy, &event);
	fvwm_interaction_coalesce(mask, &event);
	assert(event.type == KeyPress);
	FNextEvent(dpy, &event);
	assert(event.type == MotionNotify);
}

static void test_cancellation_retains_lifecycle_order(void)
{
	fvwm_interaction interaction = { 42, DefaultRootWindow(dpy), True,
		FVWM_INTERACTION_RUNNING, NULL };
	XEvent event = { 0 };

	event.type = DestroyNotify;
	event.xdestroywindow.window = interaction.client;
	FPutBackEvent(dpy, &event);
	event.type = MotionNotify;
	FPutBackEvent(dpy, &event);
	assert(fvwm_interaction_cancelled(&interaction));
	assert(interaction.result == FVWM_INTERACTION_TARGET_GONE);
	FNextEvent(dpy, &event);
	assert(event.type == MotionNotify);
	FNextEvent(dpy, &event);
	assert(event.type == DestroyNotify);
	assert(event.xdestroywindow.window == interaction.client);
	/* Cancellation is sticky even after the event is drained. */
	assert(fvwm_interaction_cancelled(&interaction));
}

int main(int argc, char **argv)
{
	assert(argc == 2);
	dpy = XOpenDisplay(argv[1]);
	assert(dpy != NULL);
	test_service_event_cache_restore();
	test_bounded_motion_and_release();
	test_cancellation_retains_lifecycle_order();
	XCloseDisplay(dpy);
	return 0;
}
