/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "fvwm/initial_window_options.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include "libs/wcontext.h"
#include "fvwm/fvwm.h"
#include "fvwm/add_window.h"
#include "fvwm/eventhandler.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/functions.h"
#include "fvwm/screen.h"
#include "fvwm/update.h"

#include <stdlib.h>
#include "fvwm/style.h"
#include "fvwm/builtins.h"
#include "fvwm/colorset.h"
#include "fvwm/focus.h"

static Bool measuring;
static unsigned contexts, grabs;
static FvwmWindow *sample;

const exec_context_t *__real_exc_create_context(
	exec_context_changes_t *, exec_context_change_mask_t);
const exec_context_t *__wrap_exc_create_context(
	exec_context_changes_t *changes, exec_context_change_mask_t mask)
{
	if (measuring && changes->type == EXCT_NULL &&
	    mask == (ECC_TYPE | ECC_FW | ECC_W | ECC_WCONTEXT) &&
	    changes->w.wcontext == C_FRAME)
		contexts++;
	return __real_exc_create_context(changes, mask);
}

void __real_MyXGrabServer(Display *);
void __wrap_MyXGrabServer(Display *display)
{
	if (measuring)
		grabs++;
	__real_MyXGrabServer(display);
}

static void command(const exec_context_t *exc, const char *text,
	FvwmWindow *window)
{
	execute_function_override_window(NULL, exc, (char *)text, NULL,
		FUNC_DONT_DEFER, window);
}

static void check_border(FvwmWindow *window, unsigned width)
{
	Atom type;
	int format;
	unsigned long count, remaining;
	unsigned char *data = NULL;
	XWindowAttributes frame, client;

	assert(window->boundary_width == width);
	assert(XGetWindowProperty(dpy, FW_W(window),
		XInternAtom(dpy, "_NET_FRAME_EXTENTS", False), 0, 4, False,
		XA_CARDINAL, &type, &format, &count, &remaining, &data) ==
		Success);
	assert(type == XA_CARDINAL && format == 32 && count == 4);
	assert(remaining == 0 && data != NULL);
	for (unsigned i = 0; i < 4; i++)
	{
		assert(((unsigned long *)data)[i] == width);
	}
	XFree(data);
	assert(XGetWindowAttributes(dpy, FW_W_FRAME(window), &frame));
	assert(XGetWindowAttributes(dpy, FW_W(window), &client));
	assert(frame.width == client.width + 2 * (int)width);
	assert(frame.height == client.height + 2 * (int)width);
}

static void passed(const char *name)
{
	printf("%s: passed\n", name);

}

static FvwmWindow *create_window(const exec_context_t *exc)
{
	XSizeHints hints = { 0 };
	exec_context_changes_t changes = { 0 };
	initial_window_options_t options = { 0 };
	const char *map_command = NULL;
	const exec_context_t *context;
	FvwmWindow *window;
	Window client = XCreateSimpleWindow(dpy, Scr.Root,
		10, 10, 100, 80, 0, 0, 0);

	XStoreName(dpy, client, "update barrier control");
	hints.flags = USPosition | USSize;
	hints.x = hints.y = 10;
	hints.width = 100;
	hints.height = 80;
	XSetWMNormalHints(dpy, client, &hints);
	XSync(dpy, False);
	changes.w.w = client;
	changes.w.wcontext = C_ROOT;
	context = exc_clone_context(exc, &changes,
		ECC_W | ECC_FW | ECC_WCONTEXT);
	options.initial_state = NormalState;
	window = AddWindow(&map_command, context, NULL, &options);
	assert(window != NULL && FW_W(window) == client);
	exc_destroy_context(context);
	return window;
}

static void flush_measured(void)
{
	contexts = grabs = 0;
	measuring = True;
	flush_window_updates();
	measuring = False;
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	Bool baseline = getenv("FVWM_UPDATE_BASELINE") != NULL;
	window_style style;
	FvwmWindow *other;

	alarm(45);
	fFvwmInStartup = False;
	command(exc, "Style * NoTitle, BorderWidth 3, HandleWidth 3", NULL);
	command(exc, "UpdateStyles", NULL);
	sample = create_window(exc);
	other = create_window(exc);
	command(exc, "Style * Colorset 81, HilightColorset 82, "
		"BorderColorset 101 102 103 104 105 106 107 108, "
		"HilightBorderColorset 111 112 113 114 115 116 117 118", NULL);
	command(exc, "UpdateStyles", NULL);

	for (int family = 0; family < 2; family++)
	{
		for (int part = 0; part < BP_SIZE; part++)
		{
			int id = (family ? 111 : 101) + part;

			update_style_colorset(id);
			lookup_style(sample, &style);
			assert(style.has_style_changed);
			assert(style.change_mask.use_border_colorset ==
				(baseline || !family));
			assert(style.change_mask.use_border_colorset_hi ==
				(baseline || family));
			command(exc, "UpdateStyles", NULL);
			assert(!Scr.flags.do_need_window_update);
			assert(sample->border_cs[part] == 101 + part);
			assert(sample->border_cs_hi[part] == 111 + part);
		}
	}
	passed("all-normal-highlight-border-references");
	update_style_colorset(199);
	lookup_style(sample, &style);
	assert(style.has_style_changed == baseline);
	assert(style.change_mask.use_border_colorset == baseline);
	assert(style.change_mask.use_border_colorset_hi == baseline);
	command(exc, "UpdateStyles", NULL);
	passed("unrelated-border-colorset-keeps-style-clean");

	command(exc, "Style * BorderColorset 201 202", NULL);
	command(exc, "UpdateStyles", NULL);
	lookup_style(sample, &style);
	for (int i = 2; i < BP_SIZE; i++)
		assert(SGET_BORDER_COLORSET(style, i) == 0);
	update_style_colorset(0);
	lookup_style(sample, &style);
	assert(style.change_mask.use_border_colorset);
	command(exc, "UpdateStyles", NULL);
	command(exc, "Style * BorderColorset 201", NULL);
	command(exc, "UpdateStyles", NULL);
	lookup_style(sample, &style);
	for (int i = 0; i < BP_SIZE; i++)
		assert(SGET_BORDER_COLORSET(style, i) == 201);
	update_style_colorset(202);
	lookup_style(sample, &style);
	assert(style.has_style_changed == baseline);
	command(exc, "UpdateStyles", NULL);
	passed("partial-zero-and-single-border-colorset-references");

	update_fvwm_colorset(199);
	assert(Scr.flags.do_need_window_update == baseline);
	contexts = grabs = 0;
	measuring = True;
	command(exc, "UpdateStyles", NULL);
	measuring = False;
	assert(grabs == (baseline ? 1U : 0U));
	printf("STYLE_UPDATE_WORK unused-colorset grabs=%u contexts=%u\n",
		grabs, contexts);
	passed("unused-colorset-does-not-schedule-global-pass");

	flush_measured();
	assert(contexts == (baseline ? 2U : 0U));
	printf("STYLE_UPDATE_WORK ordinary-update contexts=%u\n", contexts);
	passed("nonsticky-update-has-no-command-context");

	command(exc, "Style * StickyAcrossDesks", NULL);
	flush_measured();
	assert(contexts == 2);
	assert(IS_STICKY_ACROSS_DESKS(sample));
	assert(IS_STICKY_ACROSS_DESKS(other));
	command(exc, "Style * !StickyAcrossDesks", NULL);
	flush_measured();
	assert(contexts == 2);
	assert(!IS_STICKY_ACROSS_DESKS(sample));
	assert(!IS_STICKY_ACROSS_DESKS(other));
	passed("sticky-update-keeps-owned-command-context");

	command(exc, "Iconify on", sample);
	assert(IS_ICONIFIED(sample) && !IS_ICONIFIED(other));
	command(exc, "Style * StickyAcrossDesksIcon", NULL);
	flush_measured();
	assert(contexts == (baseline ? 2U : 1U));
	assert(IS_ICONIFIED(sample) && IS_ICON_STICKY_ACROSS_DESKS(sample));
	command(exc, "Style * !StickyAcrossDesksIcon", NULL);
	flush_measured();
	assert(contexts == (baseline ? 2U : 1U));
	assert(!IS_ICON_STICKY_ACROSS_DESKS(sample));
	command(exc, "Iconify off", sample);
	passed("icon-sticky-update-keeps-context-and-icon-state");

	command(exc, "Style * BorderWidth 9, HandleWidth 9", NULL);
	update_fvwm_colorset(199);
	assert(Scr.flags.do_need_window_update);
	command(exc, "UpdateStyles", NULL);
	check_border(sample, 9);
	check_border(other, 9);
	passed("unrelated-colorset-preserves-pending-style-update");

	command(exc, "TitleStyle Active Colorset 141", NULL);
	command(exc, "UpdateStyles", NULL);
	update_fvwm_colorset(141);
	assert(Scr.flags.do_need_window_update);
	update_fvwm_colorset(199);
	assert(Scr.flags.do_need_window_update);
	command(exc, "UpdateStyles", NULL);
	assert(!Scr.flags.do_need_window_update);
	passed("decor-reference-and-pending-work-preserved");

	for (int id = 81; id <= 82; id++)
	{
		update_fvwm_colorset(id);
		assert(Scr.flags.do_need_window_update);
		command(exc, "UpdateStyles", NULL);
	}
	passed("ordinary-colorset-references-still-update");

	/* Border refresh also updates colors inherited implicitly from zero. */
	command(exc, "DestroyStyle *", NULL);
	command(exc, "Style * NoTitle, BorderWidth 9, HandleWidth 9, "
		"BorderColorset 201, HilightBorderColorset 211", NULL);
	command(exc, "UpdateStyles", NULL);
	command(exc, "Colorset 0 fg green", NULL);
	update_fvwm_colorset(0);
	assert(Scr.flags.do_need_window_update);
	lookup_style(sample, &style);
	assert(style.change_mask.use_border_colorset);
	assert(style.change_mask.use_border_colorset_hi);
	command(exc, "UpdateStyles", NULL);
	assert(sample->colors.fore == GetColorset(0)->fg);
	assert(sample->hicolors.fore == GetColorset(0)->fg);
	passed("implicit-zero-colors-preserve-legacy-border-refresh");
	puts("style update work: passed");
	fflush(stdout);
	_exit(0);
}
