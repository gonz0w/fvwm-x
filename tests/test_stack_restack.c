/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "fvwm/stack_restack.h"

static Bool refuse;
static unsigned attempts;
void *__real_malloc(size_t size);

void *__wrap_malloc(size_t size)
{
	attempts++;
	return refuse ? NULL : __real_malloc(size);
}

static void setup(FvwmWindow *ring, FvwmWindow *windows, unsigned count)
{
	ring->stack_next = count ? windows : ring;
	ring->stack_prev = count ? windows + count - 1 : ring;
	for (unsigned i = 0; i < count; i++)
	{
		windows[i].stack_next = i + 1 == count ? ring : windows + i + 1;
		windows[i].stack_prev = i == 0 ? ring : windows + i - 1;
		FW_W(windows + i) = 100 + i;
		FW_W_FRAME(windows + i) = 1000 + i;
	}
}

static void test_normal_packets_and_owned_values(void)
{
	const unsigned counts[] = { 2, 3, 83, 84, 85, 166, 167, 170, 513 };
	FvwmWindow ring = { 0 };
	FvwmWindow *windows = calloc(513, sizeof(*windows));
	fvwm_stack_restack_snapshot snapshot;
	unsigned long packet[FvwmPacketMaxSize];

	assert(windows != NULL);
	for (unsigned i = 0; i < sizeof(counts) / sizeof(counts[0]); i++)
	{
		unsigned count = counts[i];
		size_t offset = 0, expected_start = 0, length;
		unsigned parts = 0;

		setup(&ring, windows, count);
		assert(fvwm_stack_restack_prepare(&snapshot, &ring,
			windows, &ring));
		assert(snapshot.count == count && !snapshot.send_raise);
		/* The copied values survive changes and retirement of live records. */
		for (unsigned j = 0; j < count; j++)
		{
			FW_W(windows + j) = 0;
			FW_W_FRAME(windows + j) = 0;
			windows[j].stack_next = windows[j].stack_prev = NULL;
		}
		while ((length = fvwm_stack_restack_packet(
			&snapshot, &offset, packet, 700 + parts)) != 0)
		{
			assert(length <= FvwmPacketMaxSize);
			assert(packet[0] == START_FLAG && packet[1] == M_RESTACK);
			assert(packet[2] == length && packet[3] == 700 + parts);
			unsigned records = (length - FvwmPacketHeaderSize) / 3;

			assert(records >= 2);
			for (unsigned j = 0; j < records; j++)
			{
				unsigned index = expected_start + j;
				unsigned long *record = packet + FvwmPacketHeaderSize + 3 * j;

				assert(index < count && record[0] == 100 + index);
				assert(record[1] == 1000 + index);
				assert(record[2] == (unsigned long)(windows + index));
			}
			expected_start += records - 1;
			assert(offset == expected_start);
			parts++;
		}
		assert(offset == count - 1 && parts > 0);
		assert(parts == (count - 2) / 83 + 1);
		offset = SIZE_MAX;
		assert(fvwm_stack_restack_packet(&snapshot, &offset, packet, 0) == 0);
		fvwm_stack_restack_finish(&snapshot);
		assert(snapshot.records == NULL && snapshot.count == 0);
		fvwm_stack_restack_finish(&snapshot);
	}
	free(windows);
}

static void test_boundaries_and_refusal(void)
{
	FvwmWindow ring = { 0 }, windows[4] = { 0 };
	fvwm_stack_restack_snapshot snapshot;
	unsigned long packet[FvwmPacketMaxSize];
	size_t offset = 0;

	setup(&ring, windows, 0);
	assert(fvwm_stack_restack_prepare(&snapshot, &ring, &ring, &ring));
	assert(snapshot.records == NULL && !snapshot.send_raise);
	assert(fvwm_stack_restack_packet(&snapshot, &offset, packet, 0) == 0);
	for (unsigned count = 1; count <= 4; count++)
	{
		setup(&ring, windows, count);
		assert(fvwm_stack_restack_prepare(&snapshot, &ring, &ring, &ring));
		assert(snapshot.send_raise && snapshot.raised[0] == 100);
		assert(snapshot.raised[1] == 1000);
		assert(snapshot.raised[2] == (unsigned long)windows);
		assert(snapshot.count == (count <= 2 ? 0 : count));
		fvwm_stack_restack_finish(&snapshot);
	}
	setup(&ring, windows, 4);
	assert(fvwm_stack_restack_prepare(&snapshot, &ring, windows, windows));
	assert(snapshot.count == 0 && !snapshot.send_raise);
	assert(!fvwm_stack_restack_prepare(&snapshot, &ring, NULL, windows));
}

static void test_allocation_refusal_and_retry(void)
{
	FvwmWindow ring = { 0 }, windows[4] = { 0 }, originals[4];
	fvwm_stack_restack_snapshot snapshot;

	setup(&ring, windows, 4);
	memcpy(originals, windows, sizeof(windows));
	unsigned before = attempts;

	refuse = True;
	assert(!fvwm_stack_restack_prepare(&snapshot, &ring, &ring, &ring));
	assert(attempts == before + 1 && snapshot.records == NULL);
	assert(snapshot.count == 0 && !snapshot.send_raise);
	assert(memcmp(originals, windows, sizeof(windows)) == 0);
	fvwm_stack_restack_finish(&snapshot);
	refuse = False;
	assert(fvwm_stack_restack_prepare(&snapshot, &ring, &ring, &ring));
	assert(snapshot.count == 4 && snapshot.send_raise);
	fvwm_stack_restack_finish(&snapshot);
	windows[1].stack_next = NULL;
	assert(!fvwm_stack_restack_prepare(&snapshot, &ring, windows, windows + 3));
	assert(snapshot.records == NULL && snapshot.count == 0);
}

int main(void)
{
	test_normal_packets_and_owned_values();
	test_boundaries_and_refusal();
	test_allocation_refusal_and_retry();
	return 0;
}
