/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "stack_restack.h"

_Static_assert(FvwmPacketMaxSize >= FvwmPacketHeaderSize + 6,
	"restack packets must fit two records");

static void copy_record(unsigned long *record, const FvwmWindow *window)
{
	record[0] = FW_W(window);
	record[1] = FW_W_FRAME(window);
	record[2] = (unsigned long)window;
}

Bool fvwm_stack_restack_prepare(
	fvwm_stack_restack_snapshot *snapshot, const FvwmWindow *ring,
	const FvwmWindow *first, const FvwmWindow *last)
{
	const FvwmWindow *window;
	size_t count = 0;

	*snapshot = (fvwm_stack_restack_snapshot){ 0 };
	if (first == NULL || last == NULL)
	{
		return False;
	}
	if (last == ring)
	{
		last = ring->stack_prev;
		if (last == ring)
		{
			return True;
		}
	}
	if (first == ring)
	{
		first = ring->stack_next;
		if (first == ring)
		{
			return True;
		}
		snapshot->send_raise = True;
		copy_record(snapshot->raised, first);
		if (first->stack_next == last)
		{
			return True;
		}
	}
	if (first == last)
	{
		return True;
	}
	for (window = first; window != ring && window != NULL;
	     window = window->stack_next)
	{
		if (count == SIZE_MAX / (3 * sizeof(unsigned long)))
		{
			goto refuse;
		}
		count++;
		if (window == last)
		{
			break;
		}
	}
	if (window != last)
	{
		goto refuse;
	}
	snapshot->records = malloc(count * 3 * sizeof(unsigned long));
	if (snapshot->records == NULL)
	{
		goto refuse;
	}
	snapshot->count = count;
	window = first;
	for (size_t i = 0; i < count; i++, window = window->stack_next)
	{
		copy_record(snapshot->records + 3 * i, window);
	}
	return True;

refuse:
	*snapshot = (fvwm_stack_restack_snapshot){ 0 };
	return False;
}

size_t fvwm_stack_restack_packet(const fvwm_stack_restack_snapshot *snapshot,
	size_t *offset, unsigned long packet[FvwmPacketMaxSize],
	unsigned long timestamp)
{
	const size_t maximum = (FvwmPacketMaxSize - FvwmPacketHeaderSize) / 3;
	size_t count;
	size_t length;

	if (snapshot->count < 2 || *offset >= snapshot->count - 1)
	{
		return 0;
	}
	count = snapshot->count - *offset;
	if (count > maximum)
	{
		count = maximum;
	}
	length = FvwmPacketHeaderSize + 3 * count;
	packet[0] = START_FLAG;
	packet[1] = M_RESTACK;
	packet[2] = length;
	packet[3] = timestamp;
	memcpy(packet + FvwmPacketHeaderSize, snapshot->records + 3 * *offset,
		3 * count * sizeof(unsigned long));
	*offset += count - 1;
	return length;
}

void fvwm_stack_restack_finish(fvwm_stack_restack_snapshot *snapshot)
{
	free(snapshot->records);
	*snapshot = (fvwm_stack_restack_snapshot){ 0 };
}
