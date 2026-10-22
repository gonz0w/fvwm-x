/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libs/fvwmlib.h"
#include "libs/Ficonv.h"
#include "libs/picture_types.h"
#include "libs/Picture.h"
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/commands.h"
#include "fvwm/decorations.h"
#include "fvwm/ewmh.h"
#include "fvwm/ewmh_intern.h"
#include "fvwm/externs.h"
#include "fvwm/screen.h"
#include "libs/monitor_types.h"
#include "ewmh_tray_probe.h"
extern ewmh_atom ewmh_atom_allowed_actions[];

static Bool measure_work, measure_policy, measure_names, large_name;
static Bool measure_icons;
static unsigned icon_allocations, icon_image_reads;
static Bool refuse_icon_image;
static unsigned static_walks, dynamic_walks, paired_walks, permissions[6];
static unsigned name_count, name_frees, name_scans, name_allocations;
static size_t name_array_bytes;
static char *converted_names[64];

XImage *__real_XGetImage(Display *, Drawable, int, int, unsigned, unsigned,
	unsigned long, int);
XImage *__wrap_XGetImage(Display *display, Drawable drawable, int x, int y,
	unsigned width, unsigned height, unsigned long planes, int format)
{
	icon_image_reads += measure_icons;
	if (measure_icons && refuse_icon_image)
		return NULL;
	return __real_XGetImage(display, drawable, x, y, width, height,
		planes, format);
}

static Bool baseline(void)
{
	return getenv("FVWM_STATE_PUBLICATION_BASELINE") != NULL;
}

void __real_EWMH_UpdateWorkArea(struct monitor *);
void __wrap_EWMH_UpdateWorkArea(struct monitor *m)
{
	paired_walks += measure_work;
	__real_EWMH_UpdateWorkArea(m);
}
void __real_ewmh_ComputeAndSetWorkArea(struct monitor *);
void __wrap_ewmh_ComputeAndSetWorkArea(struct monitor *m)
{
	static_walks += measure_work;
	__real_ewmh_ComputeAndSetWorkArea(m);
}
void __real_ewmh_HandleDynamicWorkArea(struct monitor *);
void __wrap_ewmh_HandleDynamicWorkArea(struct monitor *m)
{
	dynamic_walks += measure_work;
	__real_ewmh_HandleDynamicWorkArea(m);
}

Bool __real_is_function_allowed(int, char *, const FvwmWindow *,
	request_origin_t, Bool);
Bool __wrap_is_function_allowed(int function, char *action,
	const FvwmWindow *window, request_origin_t origin, Bool override)
{
	if (measure_policy)
	{
		unsigned slot;

		assert(action == NULL && origin == RQORIG_PROGRAM_US);
		switch (function)
		{
		case F_CLOSE: slot = 0; break;
		case F_MAXIMIZE: slot = 1; break;
		case F_MOVE: slot = 2; break;
		case F_RESIZE: slot = override ? 4 : 3; break;
		case F_ICONIFY: slot = 5; break;
		default: assert(0); slot = 0;
		}
		permissions[slot]++;
	}
	return __real_is_function_allowed(function, action, window, origin,
		override);
}

size_t __real_strlen(const char *);
size_t __wrap_strlen(const char *text)
{
	if (measure_names)
		for (unsigned i = 0; i < name_count; i++)
			if (text == converted_names[i])
			{
				name_scans++;
				if (large_name)
					return INT_MAX;
				break;
			}
	return __real_strlen(text);
}

char *__real_FiconvCharsetToUtf8(Display *, FlocaleCharset *, const char *,
	unsigned int);
char *__wrap_FiconvCharsetToUtf8(Display *display, FlocaleCharset *charset,
	const char *text, unsigned int length)
{
	char *copy;

	if (!measure_names)
		return __real_FiconvCharsetToUtf8(display, charset, text, length);
	assert(charset == NULL && length == __real_strlen(text));
	if (strcmp(text, "conversion-refused") == 0)
		return NULL;
	assert(name_count < 64);
	copy = malloc((size_t)length + 1);
	assert(copy);
	memcpy(copy, text, (size_t)length + 1);
	converted_names[name_count++] = copy;
	return copy;
}

void __real_free(void *);
void __wrap_free(void *memory)
{
	if (measure_names && memory)
		for (unsigned i = 0; i < name_count; i++)
			if (memory == converted_names[i])
			{
				name_frees++;
				converted_names[i] = NULL;
				break;
			}
	__real_free(memory);
}

void *__real_fxmalloc(size_t);
void *__wrap_fxmalloc(size_t bytes)
{
	ewmh_tray_allocation(bytes);
	icon_allocations += measure_icons;
	if (measure_names && name_allocations++ == 0)
		name_array_bytes = bytes;
	return __real_fxmalloc(bytes);
}

static void check_atoms(Window window, Atom property, const Atom *expected,
	unsigned count)
{
	Atom type;
	int format;
	unsigned long length, after;
	unsigned char *data = NULL;

	assert(XGetWindowProperty(dpy, window, property, 0, 64, False,
		XA_ATOM, &type, &format, &length, &after, &data) == Success);
	assert(type == XA_ATOM && format == 32 && length == count && !after);
	assert(memcmp(data, expected, count * sizeof(Atom)) == 0);
	XFree(data);
}

static void action_publications(Window window)
{
	Atom property = XInternAtom(dpy, "_NET_WM_ALLOWED_ACTIONS", False);
	unsigned selected = 0;

	for (unsigned round = 0; round < 80; round++)
	{
		FvwmWindow fw = { 0 };
		Atom expected[16];
		unsigned count = 0, calls = 0;

		FW_W(&fw) = window;
		fw.hints.min_width = fw.hints.min_height = 1;
		fw.hints.max_width = fw.hints.max_height = 1000;
		/* MWM function bits 1..5, independently, then override/style cases. */
		fw.functions = round ? (round & 31) << 1 : ~0U;
		fw.flags.common.s.has_mwm_override = !!(round & 32);
		if (round >= 64)
		{
			fw.functions = ~0U;
			SET_FIXED(&fw, round & 1);
			SET_SIZE_FIXED(&fw, round & 2);
			SET_IS_UNMAXIMIZABLE(&fw, round & 4);
			SET_IS_UNICONIFIABLE(&fw, round & 8);
			SET_IS_UNCLOSABLE(&fw, round & 1);
			SET_ICONIFIED(&fw, round & 2);
			SET_MAXIMIZE_FIXED_SIZE_DISALLOWED(&fw, round & 4);
			if (round & 8)
				fw.hints.max_width = fw.hints.max_height = 1;
		}
		/* Independent pre-change callback walk, retaining atom order. */
		for (ewmh_atom *atom = ewmh_atom_allowed_actions; atom->name; atom++)
			if (atom->action(&fw, NULL, NULL, 0))
				expected[count++] = atom->atom;
		memset(permissions, 0, sizeof(permissions));
		measure_policy = True;
		EWMH_SetAllowedActions(&fw);
		measure_policy = False;
		for (unsigned i = 0; i < 6; i++)
		{
			calls += permissions[i];
			assert(baseline() || permissions[i] <= 1);
		}
		if (!round)
		{
			selected = calls;
			assert(calls == (baseline() ? 9 : 6));
			assert(permissions[3] == 1 && permissions[4] == 1);
		}
		check_atoms(window, property, expected, count);
	}
	printf("STATE_ACTION_WORK {\"cases\":80,\"selected_policy_calls\":%u,"
		"\"baseline\":%u}\n", selected, baseline());
	puts("allowed-action local policy/order/override/refresh: passed");
}

static rectangle reference_area(struct monitor *m, Bool dynamic)
{
	ewmh_strut area = { m->ewmhc.BaseStrut.left, m->ewmhc.BaseStrut.right,
		m->ewmhc.BaseStrut.top, m->ewmhc.BaseStrut.bottom };
	rectangle result = { m->si->x, m->si->y, m->si->w, m->si->h };

	for (FvwmWindow *fw = Scr.FvwmRoot.next; fw; fw = fw->next)
	{
		const ewmh_strut *value = dynamic ? &fw->dyn_strut : &fw->strut;

		if (fw->m != m || !IS_STICKY_ACROSS_PAGES(fw) ||
		    DO_EWMH_IGNORE_STRUT_HINTS(fw))
			continue;
		if (value->left > area.left) area.left = value->left;
		if (value->right > area.right) area.right = value->right;
		if (value->top > area.top) area.top = value->top;
		if (value->bottom > area.bottom) area.bottom = value->bottom;
	}
	result.x += area.left;
	result.y += area.top;
	result.width -= area.left + area.right;
	result.height -= area.top + area.bottom;
	return result;
}

static void strut_publications(struct monitor *live, Window window)
{
	struct monitor monitor = *live, other = *live;
	struct screen_info info = *live->si;
	DesktopsInfo desktop = { 0 };
	FvwmWindow fw[4] = { 0 };
	Atom property = XInternAtom(dpy, "_NET_WM_STRUT", False);
	unsigned selected = 0;

	monitor.si = &info;
	monitor.Desktops = &desktop;
	memset(&monitor.ewmhc.BaseStrut, 0, sizeof(monitor.ewmhc.BaseStrut));
	info.x = -100; info.y = -200; info.w = 800; info.h = 600;
	for (unsigned i = 0; i < 4; i++)
	{
		fw[i].m = i == 2 ? &other : &monitor;
		fw[i].next = i == 3 ? NULL : fw + i + 1;
		SET_STICKY_ACROSS_PAGES(fw + i, 1);
	}
	fw[1].strut.bottom = 12;
	fw[1].dyn_strut.bottom = 16;
	fw[2].strut.left = fw[2].dyn_strut.left = 400;
	fw[3].strut.left = fw[3].dyn_strut.left = 500;
	SET_DO_EWMH_IGNORE_STRUT_HINTS(fw + 3, 1);
	FW_W(fw) = window;
	Scr.FvwmRoot.next = fw;
	for (unsigned round = 0; round < 7; round++)
	{
		unsigned long values[4] = { round == 4 ? 0 : 7, 0, 0, 0 };
		Bool change_static = round == 0 || round == 1 ||
			round == 5 || round == 6;
		Bool change_dynamic = round == 0 || round == 2 ||
			round == 4 || round == 5 || round == 6;
		rectangle expected_static, expected_dynamic;
		XEvent event = { 0 };

		fw[0].strut.left = change_static ? 2 : 7;
		fw[0].dyn_strut.left = change_dynamic ? 3 : 7;
		desktop.ewmh_working_area = (rectangle){ 1, 2, 3, 4 };
		desktop.ewmh_dyn_working_area = (rectangle){ 5, 6, 7, 8 };
		if (round == 6)
			XDeleteProperty(dpy, window, property);
		else
			XChangeProperty(dpy, window, property, XA_CARDINAL, 32,
				PropModeReplace, (unsigned char *)values, 4);
		static_walks = dynamic_walks = paired_walks = 0;
		measure_work = True;
		ewmh_WMStrut(fw, round == 5 ? NULL : &event, NULL, 0);
		measure_work = False;
		assert(fw[0].strut.left == (round == 6 ? 0 : 7));
		assert(fw[0].dyn_strut.left ==
			(round == 4 || round == 6 ? 0 : 7));
		expected_static = change_static ? reference_area(&monitor, False) :
			(rectangle){ 1, 2, 3, 4 };
		expected_dynamic = change_dynamic ? reference_area(&monitor, True) :
			(rectangle){ 5, 6, 7, 8 };
		assert(memcmp(&desktop.ewmh_working_area, &expected_static,
			sizeof(rectangle)) == 0);
		assert(memcmp(&desktop.ewmh_dyn_working_area, &expected_dynamic,
			sizeof(rectangle)) == 0);
		if (change_static && change_dynamic && !baseline())
			assert(paired_walks == 1 && !static_walks && !dynamic_walks);
		else
			assert(!paired_walks && static_walks == change_static &&
				dynamic_walks == change_dynamic);
		if (!round)
			selected = static_walks + dynamic_walks + paired_walks;
	}
	Scr.FvwmRoot.next = NULL;
	printf("STATE_STRUT_WORK {\"cases\":7,\"selected_walks\":%u,"
		"\"baseline\":%u}\n", selected, baseline());
	puts("strut paired/static/dynamic/zero/missing/filter: passed");
}

static void check_names(Atom property, DesktopsInfo *desks, unsigned count)
{
	Atom type;
	int format;
	unsigned long length, after;
	unsigned char *data = NULL;
	size_t offset = 0;

	assert(XGetWindowProperty(dpy, Scr.Root, property, 0, 65536, False,
		AnyPropertyType, &type, &format, &length, &after, &data) == Success);
	assert(format == 8 && type == XInternAtom(dpy, "UTF8_STRING", False));
	assert(!after);
	for (unsigned i = 0; i < count; i++)
	{
		const char *name = strcmp(desks[i].name, "conversion-refused") ?
			desks[i].name : "";
		size_t bytes = __real_strlen(name) + 1;

		assert(bytes <= length - offset);
		assert(memcmp(data + offset, name, bytes) == 0);
		offset += bytes;
	}
	assert(offset == length);
	XFree(data);
}

static void desktop_names(struct monitor *live)
{
	struct monitor monitor = *live;
	DesktopsInfo desktop = { 0 }, negative = { .desk = -1, .name = "skip" };
	DesktopsInfo desks[65] = { 0 };
	Atom property = XInternAtom(dpy, "_NET_DESKTOP_NAMES", False);
	char long_name[16385];
	unsigned selected_scans = 0;
	size_t selected_array = 0;

	monitor.Desktops = &desktop;
	desktop.next = &negative;
	negative.next = desks;
	memset(long_name, 'x', sizeof(long_name) - 1);
	long_name[sizeof(long_name) - 1] = '\0';
	for (unsigned round = 0; round < 3; round++)
	{
		unsigned count = round == 2 ? 64 : round == 1 ? 1 : 4;

		for (unsigned i = 0; i < 65; i++)
		{
			desks[i].desk = i;
			desks[i].name = i == 64 ? NULL : i == 0 ? "d\303\251sk" :
				i == 1 ? "" : i == 2 ? "conversion-refused" :
				i == 3 || i == 63 ? long_name : "desk";
			desks[i].next = i == 64 ? NULL : desks + i + 1;
		}
		if (!round) desks[4].name = NULL;
		if (round == 1) desks[1].desk = 3;
		name_count = name_frees = name_scans = name_allocations = 0;
		measure_names = True;
		EWMH_SetDesktopNames(&monitor);
		measure_names = False;
		if (!FiconvSupport)
		{
			assert(!name_count && !name_scans && !name_allocations);
			break;
		}
		assert(monitor.ewmhc.NumberOfDesktops == count);
		assert(name_frees == name_count);
		assert(name_count == count - (count > 2));
		assert(baseline() ? name_scans >= name_count * 2 :
			name_scans == name_count);
		assert(name_allocations == 2);
		check_names(property, desks, count);
		if (round == 2)
		{
			selected_scans = name_scans;
			selected_array = name_array_bytes;
		}
	}
	if (FiconvSupport && !baseline())
	{
		/* Simulate an unrepresentable converted length without allocating it.
		 * The existing property and every successfully converted owner survive
		 * or retire at the documented refusal point. */
		name_count = name_frees = name_scans = name_allocations = 0;
		large_name = measure_names = True;
		EWMH_SetDesktopNames(&monitor);
		large_name = measure_names = False;
		assert(name_count == 1 && name_frees == 1 && name_allocations == 1);
		check_names(property, desks, 64);
	}
	printf("STATE_DESKTOP_NAME_WORK {\"supported\":%u,\"names\":64,"
		"\"selected_scans\":%u,\"array_bytes\":%zu,\"baseline\":%u}\n",
		FiconvSupport, selected_scans, selected_array, baseline());
	puts("desktop-name lengths/UTF8/empty/refusal/ownership: passed");
}

static void check_icon_words(Window window, const CARD32 *expected,
	unsigned count)
{
	Atom type;
	int format;
	unsigned long length, after;
	unsigned char *data = NULL;
	Atom property = XInternAtom(dpy, "_NET_WM_ICON", False);

	assert(XGetWindowProperty(dpy, window, property, 0, 128, False,
		XA_CARDINAL, &type, &format, &length, &after, &data) == Success);
	assert(!after && length == count);
	if (count)
	{
		assert(type == XA_CARDINAL && format == 32);
		for (unsigned i = 0; i < count; i++)
			assert((CARD32)((unsigned long *)data)[i] == expected[i]);
	}
	else
		assert(type == None);
	XFree(data);
}

static void set_icon_words(Window window, const CARD32 *words, unsigned count)
{
	unsigned long values[128];

	assert(count <= 128);
	for (unsigned i = 0; i < count; i++)
		values[i] = words[i];
	XChangeProperty(dpy, window, XInternAtom(dpy, "_NET_WM_ICON", False),
		XA_CARDINAL, 32, PropModeReplace,
		(unsigned char *)values, count);
}

static void icon_donation(Window window)
{
	const CARD32 original[] = { 2, 1, 0xff112233, 0xff445566,
		1, 1, 0xff778899 };
	const CARD32 malformed[] = { UINT32_MAX, UINT32_MAX, 0 };
	Pixmap pixmap = XCreatePixmap(dpy, Scr.Root, 1, 1, Pdepth);
	GC gc = XCreateGC(dpy, pixmap, 0, NULL);

	assert(pixmap != None && gc != None);
	XSetForeground(dpy, gc, BlackPixel(dpy, DefaultScreen(dpy)));
	XFillRectangle(dpy, pixmap, gc, 0, 0, 1, 1);
	for (unsigned mode = 0; mode < 10; mode++)
	{
		FvwmWindow fw = { 0 };
		FvwmPicture picture = { .picture = pixmap,
			.width = 1, .height = 1, .count = 1 };
		CARD32 input[7], expected[7], *donation;
		int bytes = sizeof(input), input_bytes;
		Bool success = mode == 0 || mode >= 8;

		memcpy(input, original, sizeof(input));
		memcpy(expected, original, 4 * sizeof(CARD32));
		FW_W(&fw) = window;
		fw.mini_icon = &picture;
		fw.ewmh_icon_width = 2;
		fw.ewmh_icon_height = 1;
		fw.ewmh_mini_icon_width = fw.ewmh_mini_icon_height = 7;
		switch (mode)
		{
		case 1: picture.pending = True; break;
		case 2: picture.picture = None; break;
		case 3: picture.width = 0; break;
		case 4: picture.width = 4097; break;
		case 5: picture.height = 4097; break;
		case 6: picture.width = UINT32_MAX; break;
		case 7: refuse_icon_image = True; break;
		case 8:
			memcpy(input, malformed, sizeof(malformed));
			bytes = sizeof(malformed);
			break;
		case 9: bytes--; break;
		}
		input_bytes = bytes;
		set_icon_words(window, original, 7);
		icon_image_reads = icon_allocations = 0;
		measure_icons = True;
		donation = ewmh_SetWmIconFromPixmap(&fw, input, &bytes, True);
		measure_icons = False;
		refuse_icon_image = False;
		assert(memcmp(input, mode == 8 ? malformed : original,
			mode == 8 ? sizeof(malformed) : sizeof(original)) == 0);
		assert(picture.count == 1 && fw.mini_icon == &picture);
		assert(fw.ewmh_icon_width == 2 && fw.ewmh_icon_height == 1);
		if (success)
		{
			unsigned retained = mode == 0 ? 4 : 0;

			assert(donation != NULL && icon_image_reads == 1);
			assert(bytes == (int)((retained + 3) * sizeof(CARD32)));
			expected[retained] = expected[retained + 1] = 1;
			expected[retained + 2] = 0xff000000;
			assert(memcmp(donation, expected, bytes) == 0);
			check_icon_words(window, expected, retained + 3);
			assert(fw.ewmh_mini_icon_width == 1 &&
				fw.ewmh_mini_icon_height == 1);
			free(donation);
		}
		else
		{
			assert(donation == NULL && bytes == input_bytes);
			assert(icon_image_reads == (mode == 7 ? 1U : 0U));
			assert(icon_allocations == 0);
			check_icon_words(window, original, 7);
			assert(fw.ewmh_mini_icon_width == 7 &&
				fw.ewmh_mini_icon_height == 7);
		}
	}
	XFreeGC(dpy, gc);
	XFreePixmap(dpy, pixmap);
}

static void icon_malformed_retention(Window window)
{
	const CARD32 records[][4] = { { UINT32_MAX, UINT32_MAX, 0, 0 },
		{ 0, 1, 0, 0 }, { 2, 2, 0, 0 }, { 1, 1, 0xff112233, 2 } };

	for (unsigned mode = 0; mode < 4; mode++)
	{
		FvwmWindow fw = { 0 };

		FW_W(&fw) = window;
		fw.ewmh_icon_width = fw.ewmh_icon_height = 1;
		fw.ewmh_mini_icon_width = fw.ewmh_mini_icon_height = 2;
		set_icon_words(window, records[mode], 4);
		EWMH_DeleteWmIcon(&fw, True, False);
		/* The first complete exact record still wins before a bad suffix. */
		check_icon_words(window, mode == 3 ? records[mode] : NULL,
			mode == 3 ? 3 : 0);
		assert(fw.ewmh_icon_width == 1 && fw.ewmh_icon_height == 1);
		assert(!fw.ewmh_mini_icon_width && !fw.ewmh_mini_icon_height);
	}
}

static void icon_retention(Window window)
{
	const CARD32 icons[] = { 2, 1, 0xff112233, 0xff445566,
		1, 1, 0xff778899 };
	unsigned selected = 0;
	Bool old = getenv("FVWM_ICON_RETENTION_BASELINE") != NULL;

	if (!FMiniIconsSupported)
		return;
	for (unsigned mode = 0; mode < 3; mode++)
	{
		FvwmWindow fw = { 0 };
		Bool mini = mode != 1, icon = mode != 0;

		FW_W(&fw) = window;
		fw.ewmh_icon_width = 2;
		fw.ewmh_icon_height = 1;
		fw.ewmh_mini_icon_width = fw.ewmh_mini_icon_height = 1;
		set_icon_words(window, icons, 7);
		icon_allocations = 0;
		measure_icons = True;
		EWMH_DeleteWmIcon(&fw, mini, icon);
		measure_icons = False;
		if (mode == 2)
		{
			assert(icon_allocations == 0);
			check_icon_words(window, NULL, 0);
		}
		else
		{
			assert(icon_allocations == (old ? 3U : 2U));
			check_icon_words(window, icons + (mode ? 4 : 0),
				mode ? 3 : 4);
			selected = icon_allocations;
		}
		assert(fw.ewmh_icon_width == (icon ? 0 : 2));
		assert(fw.ewmh_icon_height == (icon ? 0 : 1));
		assert(fw.ewmh_mini_icon_width == (mini ? 0 : 1));
		assert(fw.ewmh_mini_icon_height == (mini ? 0 : 1));
	}
	if (!old)
	{
		icon_donation(window);
		icon_malformed_retention(window);
	}
	printf("STATE_ICON_RETENTION_WORK {\"cases\":%u,"
		"\"partial_delete_allocations\":%u,\"baseline\":%u}\n",
		old ? 3U : 17U, selected, old);
	puts("icon retained-span deletion/pixels/metadata/ownership: passed");
}

void ewmh_publication_probe(struct monitor *monitor)
{
	Window window;

	assert(Scr.FvwmRoot.next == NULL);
	window = XCreateSimpleWindow(dpy, Scr.Root, 0, 0, 20, 20, 0, 0, 0);
	action_publications(window);
	strut_publications(monitor, window);
	desktop_names(monitor);
	icon_retention(window);
	XDestroyWindow(dpy, window);
	XSync(dpy, False);
}
