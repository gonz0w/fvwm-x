/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/screen.h"
#include "fvwm/stack.h"
#include "fvwm/module_list.h"
#include "libs/monitor_types.h"

#define MAX_RECORDS 170
static FvwmWindow *windows;
static unsigned count, mode, packets, next_record, refresh_offset;
static unsigned refresh_packets, raises;
static Bool nested, changed;
static unsigned long original[MAX_RECORDS * 3];

void __wrap_focus_grab_buttons_on_layer(int layer)
{
}

static void change_ring(void)
{
	unsigned target = count > 84 ? 83 : 0;

	changed = True;
	nested = True;
	/* Model the ordinary command that a synchronous unlock wait executes. */
	LowerWindow(windows + target, False);
	nested = False;
	assert(Scr.FvwmRoot.stack_prev == windows + target);
}

void __wrap_PositiveWrite(fmodule *module, unsigned long *packet, int size)
{
	if (nested)
	{
		return;
	}
	assert(packet[0] == START_FLAG && packet[2] * sizeof(*packet) == size);
	assert(packet[2] <= FvwmPacketMaxSize);
	if (packet[1] == M_RAISE_WINDOW)
	{
		assert(memcmp(packet + FvwmPacketHeaderSize, original,
			3 * sizeof(*packet)) == 0);
		raises++;
		if (mode == 2 && !changed)
		{
			change_ring();
		}
		return;
	}
	if (packet[1] != M_RESTACK)
	{
		return;
	}
	unsigned records = (packet[2] - FvwmPacketHeaderSize) / 3;

	assert(records >= 2 && next_record + records <= count);
	assert(memcmp(packet + FvwmPacketHeaderSize,
		original + next_record * 3, records * 3 * sizeof(*packet)) == 0);
	next_record += records - 1;
	packets++;
	if (mode == 1 && !changed)
	{
		change_ring();
	}
}

void __wrap_module_output_enqueue(
	fmodule *module, unsigned long *packet, int size)
{
	FvwmWindow *window = Scr.FvwmRoot.stack_next;
	unsigned records = (packet[2] - FvwmPacketHeaderSize) / 3;

	assert(!nested && changed && packet[1] == M_RESTACK);
	assert(packet[0] == START_FLAG && packet[2] * sizeof(*packet) == size);
	assert(packet[2] <= FvwmPacketMaxSize && records >= 2);
	for (unsigned i = 0; i < refresh_offset; i++)
	{
		window = window->stack_next;
	}
	for (unsigned i = 0; i < records; i++, window = window->stack_next)
	{
		unsigned long *record = packet + FvwmPacketHeaderSize + 3 * i;

		assert(window != &Scr.FvwmRoot);
		assert(record[0] == FW_W(window) && record[1] == FW_W_FRAME(window));
		assert(record[2] == (unsigned long)window);
	}
	refresh_offset += records - 1;
	refresh_packets++;
}

void __real_module_output_enqueue(fmodule *, unsigned long *, int);
static void check_copied_async_queue(void)
{
	fmodule *module = module_create();
	int descriptors[2];
	unsigned long packet[10] = { START_FLAG, M_RESTACK, 10, 17,
		100, 200, 300, 101, 201, 301 };
	unsigned long expected[10];
	unsigned long received[10];

	assert(pipe(descriptors) == 0);
	MOD_WRITEFD(module) = descriptors[1];
	module->xSyncMask.m1 = M_RESTACK;
	memcpy(expected, packet, sizeof(packet));
	/* No unlock input exists: this must copy and return without waiting. */
	__real_module_output_enqueue(module, packet, sizeof(packet));
	assert(!MOD_IS_RETIRED(module) && module->xinputRefs == 0);
	assert(fqueue_get_length(&MOD_PIPEQUEUE(module)) == 1);
	assert(MOD_PIPEQUEUE_BYTES(module) >= sizeof(packet));
	memset(packet, 0, sizeof(packet));
	FlushMessageQueue(module);
	assert(FQUEUE_IS_EMPTY(&MOD_PIPEQUEUE(module)));
	assert(MOD_PIPEQUEUE_BYTES(module) == 0);
	assert(read(descriptors[0], received, sizeof(received)) == sizeof(received));
	assert(memcmp(expected, received, sizeof(received)) == 0);
	module_destroy(module);
	close(descriptors[0]);
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	Window frames[MAX_RECORDS];
	fmodule *module = module_create();

	count = (unsigned)strtoul(getenv("FVWM_RESTACK_COUNT"), NULL, 10);
	mode = (unsigned)strtoul(getenv("FVWM_RESTACK_MODE"), NULL, 10);
	assert(count >= 1 && count <= MAX_RECORDS && mode <= 2);
	check_copied_async_queue();
	windows = calloc(count, sizeof(*windows));
	assert(windows != NULL);
	Scr.FvwmRoot.next = windows;
	Scr.FvwmRoot.stack_next = windows;
	Scr.FvwmRoot.stack_prev = windows + count - 1;
	for (unsigned i = 0; i < count; i++)
	{
		FvwmWindow *window = windows + i;

		window->prev = i == 0 ? &Scr.FvwmRoot : window - 1;
		window->next = i + 1 == count ? NULL : window + 1;
		window->stack_prev = i == 0 ? &Scr.FvwmRoot : window - 1;
		window->stack_next = i + 1 == count ? &Scr.FvwmRoot : window + 1;
		window->layer = 4;
		window->m = monitor_get_current();
		assert(window->m != NULL);
		window->Desk = window->m->virtual_scr.CurrentDesk;
		FW_W(window) = XCreateSimpleWindow(
			dpy, Scr.Root, 0, 0, 10, 10, 0, 0, 0);
		FW_W_FRAME(window) = FW_W(window);
		frames[i] = FW_W(window);
		original[3 * i] = FW_W(window);
		original[3 * i + 1] = FW_W_FRAME(window);
		original[3 * i + 2] = (unsigned long)window;
		assert(XSaveContext(dpy, FW_W(window), FvwmContext,
			(char *)window) == 0);
	}
	XRestackWindows(dpy, frames, (int)count);
	module_register(module);
	if (mode == 2)
	{
		BroadcastRestackThisWindow(windows);
	}
	else
	{
		BroadcastRestackAllWindows();
	}
	/* This-window prefix emits only Raise for its adjacent boundaries. */
	Bool restack_expected = count > 1 && mode != 2;

	assert(raises == (mode == 2 ? 1U : 0U));
	assert(packets == (restack_expected ? (count - 2) / 83 + 1 : 0));
	assert(next_record == (restack_expected ? count - 1 : 0));
	fvwm_stack_restack_publication_flush();
	assert(refresh_packets == (changed && count > 1 ?
		(count - 2) / 83 + 1 : 0));
	assert(refresh_offset == (changed && count > 1 ? count - 1 : 0));
	unsigned before = refresh_packets;

	fvwm_stack_restack_publication_flush();
	assert(refresh_packets == before);
	XSync(dpy, False);
	puts("owned-restack-callback-and-final-publication: passed");
	fflush(stdout);
	_exit(0);
}
