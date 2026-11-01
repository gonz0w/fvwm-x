/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "script_message.h"
#include <stdlib.h>
#include <string.h>

bool script_message_enqueue(script_message_queue *queue,
	unsigned long target, char *text)
{
	if (queue->retired || queue->count == SCRIPT_MESSAGE_LIMIT ||
	    target == 0 || text == NULL)
	{
		return false;
	}
	size_t length = strnlen(text, SCRIPT_MESSAGE_MAX_BYTES);

	if (length == SCRIPT_MESSAGE_MAX_BYTES)
	{
		return false;
	}
	queue->entries[queue->count++] =
		(script_message){target, text, length + 1};
	return true;
}

char *script_message_take(script_message_queue *queue, unsigned long target)
{
	for (size_t i = 0; i < queue->count; i++)
	{
		if (queue->entries[i].target == target)
		{
			char *text = queue->entries[i].text;

			queue->count--;
			memmove(&queue->entries[i], &queue->entries[i + 1],
				(queue->count - i) * sizeof(queue->entries[0]));
			/* The compacted tail never retains an alias to a live owner. */
			queue->entries[queue->count] = (script_message){0};
			return text;
		}
	}
	return NULL;
}

void script_message_clear(script_message_queue *queue)
{
	while (queue->count > 0)
	{
		queue->count--;
		free(queue->entries[queue->count].text);
		queue->entries[queue->count] = (script_message){0};
	}
}

const script_message *script_message_peek(const script_message_queue *queue,
	unsigned long target)
{
	for (size_t i = 0; i < queue->count; i++)
	{
		if (queue->entries[i].target == target)
			return &queue->entries[i];
	}
	return NULL;
}

size_t script_message_targets(const script_message_queue *queue,
	unsigned long *targets, size_t capacity)
{
	size_t count = 0;

	for (size_t i = 0; i < queue->count; i++)
	{
		size_t j;

		for (j = 0; j < count; j++)
			if (targets[j] == queue->entries[i].target)
				break;
		if (j != count)
			continue;
		if (count == capacity)
			break;
		targets[count++] = queue->entries[i].target;
	}
	return count;
}

void script_message_retire(script_message_queue *queue)
{
	script_message_clear(queue);
	queue->retired = true;
}

void script_message_open(script_message_queue *queue)
{
	queue->retired = false;
}
