/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/script_message.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static char *owned_text(const char *text)
{
	char *copy = malloc(strlen(text) + 1);

	assert(copy != NULL);
	strcpy(copy, text);
	return copy;
}

static void check_tail(const script_message_queue *queue)
{
	assert(queue->count <= SCRIPT_MESSAGE_LIMIT);
	for (size_t i = queue->count; i < SCRIPT_MESSAGE_LIMIT; i++)
	{
		assert(queue->entries[i].target == 0);
		assert(queue->entries[i].text == NULL);
	}
	for (size_t i = 0; i < queue->count; i++)
	{
		assert(queue->entries[i].target != 0);
		assert(queue->entries[i].text != NULL);
		for (size_t j = i + 1; j < queue->count; j++)
		{
			assert(queue->entries[i].text != queue->entries[j].text);
		}
	}
}

static void take(script_message_queue *queue, unsigned long target,
	const char *expected)
{
	char *text = script_message_take(queue, target);

	assert(text != NULL && strcmp(text, expected) == 0);
	free(text);
	check_tail(queue);
}

static void test_fifo_and_removal(void)
{
	script_message_queue queue = {0};
	char *first = owned_text("first literal # $ value");

	assert(script_message_enqueue(&queue, 11, first));
	assert(script_message_enqueue(&queue, 22, owned_text("middle")));
	assert(script_message_enqueue(&queue, 11, owned_text("same target")));
	assert(script_message_enqueue(&queue, 33, owned_text("last")));
	assert(script_message_take(&queue, 44) == NULL);
	assert(script_message_take(&queue, 0) == NULL);
	assert(queue.count == 4 && queue.entries[0].text == first);
	check_tail(&queue);
	take(&queue, 22, "middle");
	assert(queue.count == 3 && queue.entries[0].target == 11);
	assert(queue.entries[1].target == 11 && queue.entries[2].target == 33);
	take(&queue, 33, "last");
	take(&queue, 11, "first literal # $ value");
	take(&queue, 11, "same target");
	assert(queue.count == 0 && script_message_take(&queue, 11) == NULL);
	check_tail(&queue);
}

static void test_full_refusal_and_clear(void)
{
	script_message_queue queue = {0};
	char *refused = owned_text("caller still owns me");

	assert(!script_message_enqueue(&queue, 0, refused));
	assert(!script_message_enqueue(&queue, 1, NULL));
	assert(queue.count == 0);
	for (size_t i = 0; i < SCRIPT_MESSAGE_LIMIT; i++)
	{
		assert(script_message_enqueue(&queue, i + 1, owned_text("queued")));
	}
	assert(!script_message_enqueue(&queue, 41, refused));
	assert(strcmp(refused, "caller still owns me") == 0);
	free(refused);
	check_tail(&queue);
	take(&queue, 20, "queued");
	assert(script_message_enqueue(&queue, 41, owned_text("replacement")));
	assert(queue.count == SCRIPT_MESSAGE_LIMIT);
	assert(queue.entries[SCRIPT_MESSAGE_LIMIT - 1].target == 41);
	script_message_clear(&queue);
	assert(queue.count == 0);
	check_tail(&queue);
	script_message_clear(&queue);
	assert(script_message_enqueue(&queue, 1, owned_text("reused")));
	take(&queue, 1, "reused");
	script_message_clear(&queue);
}

int main(void)
{
	test_fifo_and_removal();
	test_full_refusal_and_clear();
	return 0;
}
