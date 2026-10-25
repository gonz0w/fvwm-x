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
	if (queue->count == SCRIPT_MESSAGE_LIMIT || target == 0 || text == NULL)
	{
		return false;
	}
	queue->entries[queue->count++] = (script_message){target, text};
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
