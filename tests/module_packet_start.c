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
#include <unistd.h>
#include "libs/wcontext.h"
#include "fvwm/fvwm.h"
#include "libs/vpacket.h"
#include "libs/Parse.h"
#include "libs/module_monitor.h"
#include "fvwm/add_window.h"
#include "fvwm/commands.h"
#include "fvwm/decorations.h"
#include "fvwm/externs.h"
#include "fvwm/screen.h"
#include "fvwm/execcontext.h"
#include "fvwm/module_list.h"
#include "fvwm/module_interface.h"
#include "libs/monitor_types.h"
#include "libs/picture_types.h"
#define WORDS_16 0UL, 1UL, 2UL, 3UL, 4UL, 5UL, 6UL, 7UL, \
	8UL, 9UL, 10UL, 11UL, 12UL, 13UL, 14UL, 15UL
#define WORDS_256 WORDS_16, WORDS_16, WORDS_16, WORDS_16, \
	WORDS_16, WORDS_16, WORDS_16, WORDS_16, \
	WORDS_16, WORDS_16, WORDS_16, WORDS_16, \
	WORDS_16, WORDS_16, WORDS_16, WORDS_16
#include "fvwm/initial_window_options.h"
static unsigned long captured[FvwmPacketMaxSize];
static size_t captured_size;
static unsigned captures;
static unsigned allocations;
static Bool watch_allocations;
static const char *expected_monitor_name;
static unsigned monitor_records, matching_monitor_records;
static Bool check_monitor_records;

void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size)
{
	if (watch_allocations)
	{
		allocations++;
	}
	return __real_malloc(size);
}

void __wrap_PositiveWrite(fmodule *module, unsigned long *packet, int size)
{
	assert(size >= FvwmPacketHeaderSize * sizeof(*packet));
	assert((size_t)size <= sizeof(captured));
	assert(packet[0] == START_FLAG);
	assert(packet[2] * sizeof(*packet) == (size_t)size);
	memcpy(captured, packet, size);
	captured_size = size;
	captures++;
	if (check_monitor_records && packet[1] == M_CONFIG_INFO &&
		packet[2] > FvwmPacketHeaderSize + 3)
	{
		char *text = (char *)&captured[FvwmPacketHeaderSize + 3];
		size_t capacity = captured_size -
			(FvwmPacketHeaderSize + 3) * sizeof(*captured);
		assert(memchr(text, '\0', capacity) != NULL);
		if (strncmp(text, "Monitor ", 8) == 0)
		{
			char *name = NULL;
			int values[MODULE_MONITOR_VALUES];
			char *rest = GetNextToken(text + 8, &name);
			assert(name != NULL);
			assert(module_parse_decimal_integers(rest, values,
				MODULE_MONITOR_VALUES));
			monitor_records++;
			if (expected_monitor_name != NULL &&
				strcmp(name, expected_monitor_name) == 0)
			{
				matching_monitor_records++;
			}
			free(name);
		}
	}
}

static void check_monitor_names(fmodule *module)
{
	struct monitor *monitor = monitor_get_current();
	assert(monitor != NULL && monitor->si != NULL);
	const char *original = monitor->si->name;
	const char *names[] = {"owned-DP-1", "owned two monitors",
		"owned'quote\"tick`", "owned\\backslash"};
	unsigned ordinary_records = 0;
	check_monitor_records = True;
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
	{
		monitor->si->name = expected_monitor_name = names[i];
		monitor_records = matching_monitor_records = 0;
		BroadcastMonitorList(module);
		assert(matching_monitor_records == 1);
		if (i == 0)
		{
			ordinary_records = monitor_records;
		}
		assert(monitor_records == ordinary_records);
	}
	char too_long[MAX_TOKEN_LENGTH + 1];
	memset(too_long, 'n', sizeof(too_long) - 1);
	too_long[sizeof(too_long) - 1] = '\0';
	monitor->si->name = expected_monitor_name = too_long;
	monitor_records = matching_monitor_records = 0;
	BroadcastMonitorList(module);
	assert(matching_monitor_records == 0);
	assert(monitor_records + 1 == ordinary_records);
	monitor->si->name = original;
	expected_monitor_name = NULL;
	check_monitor_records = False;
	puts("module-monitor-quoted-round-trip-and-oversize-refusal: passed");
}

static void check_name(const char *name, unsigned items)
{
	const char *received = (const char *)&captured[
		FvwmPacketHeaderSize + items];
	size_t capacity = captured_size -
		(FvwmPacketHeaderSize + items) * sizeof(*captured);
	size_t length = strlen(name);
	size_t retained = length < capacity - 1 ? length : capacity - 1;

	assert(captured[4] == 1 && captured[5] == 2 && captured[6] == 3);
	assert(memcmp(received, name, retained) == 0);
	for (size_t i = retained; i < capacity; i++)
	{
		assert(received[i] == 0);
	}
}

static void check_configure(fmodule *module, const FvwmWindow *window)
{
	ConfigWinPacket expected = { 0 };

	expected.w = FW_W(window);
	expected.frame = FW_W_FRAME(window);
	expected.fvwmwin = (unsigned long *)window;
	expected.frame_x = window->g.frame.x;
	expected.frame_y = window->g.frame.y;
	expected.frame_width = window->g.frame.width;
	expected.frame_height = window->g.frame.height;
	expected.desk = window->Desk;
	expected.monitor_id = window->m->si->rr_output;
	expected.layer = window->layer;
	expected.hints_base_width = window->hints.base_width;
	expected.hints_base_height = window->hints.base_height;
	expected.hints_width_inc = window->hints.width_inc;
	expected.hints_height_inc = window->hints.height_inc;
	expected.orig_hints_width_inc = window->orig_hints.width_inc;
	expected.orig_hints_height_inc = window->orig_hints.height_inc;
	expected.hints_min_width = window->hints.min_width;
	expected.hints_min_height = window->hints.min_height;
	expected.hints_max_width = window->hints.max_width;
	expected.hints_max_height = window->hints.max_height;
	expected.icon_w = FW_W_ICON_TITLE(window);
	expected.icon_pixmap_w = FW_W_ICON_PIXMAP(window);
	expected.hints_win_gravity = window->hints.win_gravity;
	expected.TextPixel = window->colors.fore;
	expected.BackPixel = window->colors.back;
	expected.ewmh_hint_layer = window->ewmh_hint_layer;
	expected.ewmh_hint_desktop = window->ewmh_hint_desktop;
	expected.ewmh_window_type = window->ewmh_window_type;
	expected.title_height = window->title_thickness;
	expected.border_width = window->boundary_width;
	expected.flags = window->flags;
	expected.allowed_actions.is_movable = is_function_allowed(
		F_MOVE, NULL, window, RQORIG_PROGRAM_US, False);
	expected.allowed_actions.is_deletable = is_function_allowed(
		F_DELETE, NULL, window, RQORIG_PROGRAM_US, False);
	expected.allowed_actions.is_destroyable = is_function_allowed(
		F_DESTROY, NULL, window, RQORIG_PROGRAM_US, False);
	expected.allowed_actions.is_closable = is_function_allowed(
		F_CLOSE, NULL, window, RQORIG_PROGRAM_US, False);
	expected.allowed_actions.is_maximizable = is_function_allowed(
		F_MAXIMIZE, NULL, window, RQORIG_PROGRAM_US, False);
	expected.allowed_actions.is_resizable = is_function_allowed(
		F_RESIZE, NULL, window, RQORIG_PROGRAM_US, False);
	expected.allowed_actions.is_iconifiable = is_function_allowed(
		F_ICONIFY, NULL, window, RQORIG_PROGRAM_US, False);

	for (unsigned broadcast = 0; broadcast < 2; broadcast++)
	{
		unsigned before = captures;
		allocations = 0;
		watch_allocations = True;
		if (broadcast)
		{
			BroadcastConfig(M_CONFIGURE_WINDOW, window);
		}
		else
		{
			SendConfig(module, M_CONFIGURE_WINDOW, window);
		}
		watch_allocations = False;
		assert(allocations == 0 && captures == before + 1);
		assert(captured[1] == M_CONFIGURE_WINDOW);
		size_t bytes = captured_size -
			FvwmPacketHeaderSize * sizeof(*captured);
		assert(bytes >= sizeof(expected));
		assert(memcmp(captured + FvwmPacketHeaderSize,
			&expected, sizeof(expected)) == 0);
		for (size_t i = sizeof(expected); i < bytes; i++)
		{
			assert(((unsigned char *)(captured +
				FvwmPacketHeaderSize))[i] == 0);
		}
	}
}

static void check_managed_configure(
	fmodule *module, const exec_context_t *exc)
{
	XSizeHints hints = { 0 };
	exec_context_changes_t changes = { 0 };
	initial_window_options_t options = { 0 };
	const char *map_command = NULL;
	const exec_context_t *context;
	Window client = XCreateSimpleWindow(dpy, Scr.Root,
		10, 10, 100, 80, 0, 0, 0);

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
	FvwmWindow *window = AddWindow(&map_command, context, NULL, &options);
	assert(window != NULL && FW_W(window) == client);
	exc_destroy_context(context);
	check_configure(module, window);

	/* Exercise native signed widening and the consumer's short/raw layout. */
	FvwmWindow sample = *window;
	sample.g.frame.x = -32768;
	sample.g.frame.y = 32767;
	sample.g.frame.width = 65535;
	sample.g.frame.height = 1;
	sample.Desk = 3;
	sample.hints.base_width = -1;
	sample.hints.base_height = INT_MAX;
	sample.ewmh_hint_desktop = ~0UL;
	sample.title_thickness = 65535;
	sample.boundary_width = 65534;
	check_configure(module, &sample);
	struct monitor monitor = *window->m;
	struct screen_info screen = *window->m->si;
	const RROutput outputs[] = { 0x7fffffffUL, 0x80000000UL, 0xffffffffUL };
	monitor.si = &screen;
	sample.m = &monitor;
	for (unsigned i = 0; i < sizeof(outputs) / sizeof(*outputs); i++)
	{
		screen.rr_output = outputs[i];
		check_configure(module, &sample);
	}
	puts("module-configure-consumer-layout-and-window-identity: passed");
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	fmodule *module = module_create();
	char name[FvwmPacketMaxSize_byte * 2];
	const unsigned lengths[] = { 0, 1, 7, 8, 9, 127, sizeof(name) - 1 };
	const unsigned counts[] = { 0, 1, 3,
		FvwmPacketMaxSize - FvwmPacketHeaderSize,
		FvwmPacketMaxSize - FvwmPacketHeaderSize + 1,
		FvwmPacketMaxSize * 2 };

	module_register(module);
	for (unsigned i = 0; i < sizeof(counts) / sizeof(*counts); i++)
	{
		unsigned retained = counts[i] < FvwmPacketMaxSize -
			FvwmPacketHeaderSize ? counts[i] :
			FvwmPacketMaxSize - FvwmPacketHeaderSize;

		SendPacket(module, M_NEW_PAGE, counts[i], WORDS_256);
		assert(captured[2] == FvwmPacketHeaderSize + retained);
		for (unsigned j = 0; j < retained; j++)
		{
			assert(captured[FvwmPacketHeaderSize + j] == j % 16);
		}
		unsigned before = captures;
		BroadcastPacket(M_NEW_PAGE, counts[i], WORDS_256);
		assert(captures == before + 1);
		assert(captured[2] == FvwmPacketHeaderSize + retained);
	}
	for (unsigned i = 0; i < sizeof(lengths) / sizeof(*lengths); i++)
	{
		memset(name, 'x', lengths[i]);
		name[lengths[i]] = '\0';
		allocations = 0;
		watch_allocations = True;
		SendName(module, M_WINDOW_NAME, 1, 2, 3, name);
		watch_allocations = False;
		assert(allocations == 0);
		check_name(name, 3);
		watch_allocations = True;
		BroadcastName(M_WINDOW_NAME, 1, 2, 3, name);
		watch_allocations = False;
		assert(allocations == 0);
		check_name(name, 3);
		watch_allocations = True;
		SendFvwmPicture(module, M_MINI_ICON, 1, 2, 3, NULL, name);
		watch_allocations = False;
		assert(allocations == 0);
		check_name(name, 9);
		for (unsigned j = 7; j < 13; j++)
		{
			assert(captured[j] == 0);
		}
		watch_allocations = True;
		BroadcastFvwmPicture(M_MINI_ICON, 1, 2, 3, NULL, name);
		watch_allocations = False;
		assert(allocations == 0);
		check_name(name, 9);
	}
	FvwmPicture picture = { .width = 65535, .height = 1, .depth = 24,
		.picture = 17, .mask = 19, .alpha = 23 };
	const unsigned long metadata[] = { 65535, 1, 24, 17, 19, 23 };
	for (unsigned i = 0; i < sizeof(lengths) / sizeof(*lengths); i++)
	{
		memset(name, 'p', lengths[i]);
		name[lengths[i]] = '\0';
		allocations = 0;
		watch_allocations = True;
		SendFvwmPicture(module, M_MINI_ICON, 1, 2, 3, &picture, name);
		watch_allocations = False;
		assert(allocations == 0);
		check_name(name, 9);
		assert(memcmp(captured + 7, metadata, sizeof(metadata)) == 0);
		watch_allocations = True;
		BroadcastFvwmPicture(M_MINI_ICON, 1, 2, 3, &picture, name);
		watch_allocations = False;
		assert(allocations == 0);
		check_name(name, 9);
		assert(memcmp(captured + 7, metadata, sizeof(metadata)) == 0);
	}
	unsigned before = captures;
	SendName(module, M_WINDOW_NAME, 1, 2, 3, NULL);
	BroadcastName(M_WINDOW_NAME, 1, 2, 3, NULL);
	SendFvwmPicture(module, M_MINI_ICON, 1, 2, 3, NULL, NULL);
	BroadcastFvwmPicture(M_MINI_ICON, 1, 2, 3, NULL, NULL);
	assert(captures == before);
	check_monitor_names(module);
	check_managed_configure(module, exc);
	module_kill_all();
	puts("module-packet-bounds-and-allocation-free-names: passed");
	fflush(stdout);
	_exit(0);
}
