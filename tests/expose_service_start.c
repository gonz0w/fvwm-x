/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/screen.h"
#include "fvwm/events.h"
#include "fvwm/icons.h"
#include "fvwm/menus.h"
#include "libs/FEvent.h"

static FvwmWindow icons[2], menu;
static unsigned draws, menu_draws, collections, maximum_selected;
static Bool nested_destroy;
static XEvent last_draw;
static Bool covered[20][100];
static Bool record_damage;

extern int __real_FCollectQueuedExposes(Display *, Window, XEvent *);
extern void __real_DrawIconWindow(FvwmWindow *, Bool, Bool, Bool, Bool,
	XEvent *);
extern Bool __real_menu_expose(XEvent *, FvwmWindow *);

static XEvent expose(Window window, int x, int y)
{
	return (XEvent){.xexpose = {.type = Expose, .display = dpy,
		.window = window, .x = x, .y = y, .width = 1, .height = 1}};
}

int __wrap_FCollectQueuedExposes(Display *display, Window window,
	XEvent *events)
{
	int count = __real_FCollectQueuedExposes(display, window, events);

	assert(count >= 0 && count <= FEV_EXPOSE_BATCH_MAX);
	collections++;
	if ((unsigned)count > maximum_selected)
	{
		maximum_selected = count;
	}
	return count;
}

void __wrap_DrawIconWindow(FvwmWindow *fw, Bool title, Bool pixmap,
	Bool focus, Bool reset, XEvent *event)
{
	if (fw != icons && fw != icons + 1)
	{
		__real_DrawIconWindow(fw, title, pixmap, focus, reset, event);
		return;
	}
	assert(event && event->type == Expose);
	assert(event->xexpose.window == FW_W_ICON_TITLE(fw));
	last_draw = *event;
	if (record_damage)
	{
		for (int y = 0; y < 20; y++)
		{
			for (int x = 0; x < 100; x++)
			{
				if (x >= event->xexpose.x &&
				    x < event->xexpose.x + event->xexpose.width &&
				    y >= event->xexpose.y &&
				    y < event->xexpose.y + event->xexpose.height)
				{
					covered[y][x] = True;
				}
			}
		}
	}
	draws++;
	if (nested_destroy)
	{
		XEvent inner = expose(FW_W_ICON_TITLE(icons + 1), 7, 9);
		XEvent before, after;

		nested_destroy = False;
		/* Resolve later queued events through current X context ownership. */
		XDeleteContext(dpy, FW_W_ICON_TITLE(icons), FvwmContext);
		XDestroyWindow(dpy, FW_W_ICON_TITLE(icons));
		fev_get_last_event(&before);
		FPutBackEvent(dpy, &inner);
		fev_fake_event(&before);
		handle_all_expose();
		fev_get_last_event(&after);
		assert(memcmp(&before, &after, sizeof(before)) == 0);
	}
}

Bool __wrap_menu_expose(XEvent *event, FvwmWindow *fw)
{
	if (event->xexpose.window != FW_W(&menu))
	{
		return __real_menu_expose(event, fw);
	}
	menu_draws++;
	last_draw = *event;
	return True;
}

static void drain(void)
{
	XSync(dpy, True);
}

static void test_merge_and_fifo(void)
{
	XEvent merged = expose(FW_W_ICON_TITLE(icons), 0, 0);
	XEvent events[40], event;
	Window first = FW_W_ICON_TITLE(icons);
	Window other = FW_W_ICON_TITLE(icons + 1);

	drain();
	for (int index = 0; index < 40; index++)
	{
		events[index] = expose(index % 2 ? other : first,
			index + 1, index + 2);
		events[index].xany.serial = index;
	}
	for (int index = 39; index >= 0; index--)
	{
		FPutBackEvent(dpy, events + index);
	}
	flush_accumulate_expose(first, &merged);
	assert(merged.xexpose.x == 0 && merged.xexpose.y == 0);
	assert(merged.xexpose.width == 32 && merged.xexpose.height == 33);
	assert(FQLength(dpy) == 24);
	for (int index = 0; index < 40; index++)
	{
		if (index < 32 && index % 2 == 0)
		{
			continue;
		}
		FNextEvent(dpy, &event);
		assert(event.xany.serial == (unsigned long)index);
		assert(event.xany.window == events[index].xany.window);
	}
	puts("expose-merge-rectangle-and-foreign-window-FIFO: passed");
}

static void test_backlog_and_continuation(void)
{
	XEvent event, current = {.type = ClientMessage};
	unsigned initial_draws;

	drain();
	event = (XEvent){.xproperty = {.type = PropertyNotify,
		.window = Scr.NoFocusWin, .time = 999999}};
	FPutBackEvent(dpy, &event);
	for (int index = 0; index < 2000; index++)
	{
		event = expose(FW_W_ICON_TITLE(icons), index % 100, index / 100);
		FPutBackEvent(dpy, &event);
	}
	fev_fake_event(&current);
	Time timestamp = fev_get_evtime();
	initial_draws = draws;
	record_damage = True;
	handle_all_expose();
	assert(draws - initial_draws == 32);
	assert(FQLength(dpy) == 945);
	fev_get_last_event(&event);
	assert(memcmp(&current, &event, sizeof(current)) == 0);
	assert(fev_get_evtime() == timestamp);
	int remaining = FQLength(dpy), properties = 0;
	while (remaining-- > 0)
	{
		FNextEvent(dpy, &event);
		if (event.type == PropertyNotify)
		{
			properties++;
			assert(event.xproperty.time == 999999);
		}
		else
		{
			assert(event.type == Expose);
			dispatch_event(&event);
			remaining = FQLength(dpy);
		}
	}
	assert(properties == 1 && FQLength(dpy) == 0);
	record_damage = False;
	for (int y = 0; y < 20; y++)
	{
		for (int x = 0; x < 100; x++)
		{
			assert(covered[y][x]);
		}
	}
	printf("expose-backlog: 32 outer redraws, 944 deferred, "
		"maximum collection %u; complete damage/continuation passed\n",
		maximum_selected);
}

static void test_tearoff_and_nested_retirement(void)
{
	XEvent event;
	unsigned initial = draws;

	drain();
	event = expose(FW_W(&menu), 3, 4);
	FPutBackEvent(dpy, &event);
	handle_all_expose();
	assert(menu_draws == 1 && last_draw.xexpose.x == 3);
	drain();
	event = expose(FW_W_ICON_TITLE(icons), 1, 2);
	FPutBackEvent(dpy, &event);
	FPutBackEvent(dpy, &event);
	nested_destroy = True;
	handle_all_expose();
	/* First outer draw and nested second-icon draw; the retired outer
	 * window has no current identity and is not redrawn a second time. */
	assert(draws - initial == 2 && !nested_destroy);
	assert(last_draw.xexpose.window == FW_W_ICON_TITLE(icons + 1));
	puts("expose-tearoff-routing-nested-cache-and-retirement: passed");
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	for (int index = 0; index < 2; index++)
	{
		FW_W_ICON_TITLE(icons + index) = XCreateSimpleWindow(dpy,
			Scr.Root, 0, 0, 100, 100, 0, 0, 0);
		XSaveContext(dpy, FW_W_ICON_TITLE(icons + index), FvwmContext,
			(char *)(icons + index));
	}
	FW_W(&menu) = XCreateSimpleWindow(dpy, Scr.Root, 0, 0,
		100, 100, 0, 0, 0);
	SET_TEAR_OFF_MENU(&menu, 1);
	XSaveContext(dpy, FW_W(&menu), FvwmContext, (char *)&menu);
	test_merge_and_fifo();
	test_backlog_and_continuation();
	test_tearoff_and_nested_retirement();
	printf("expose-service: %u bounded collections passed\n", collections);
	fflush(stdout);
	_exit(0);
}
