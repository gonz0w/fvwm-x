#ifndef FVWMSCRIPT_SCRIPT_MESSAGE_H
#define FVWMSCRIPT_SCRIPT_MESSAGE_H

#include <stdbool.h>
#include <stddef.h>

#define SCRIPT_MESSAGE_LIMIT 40

typedef struct
{
	unsigned long target;
	char *text;
} script_message;

typedef struct
{
	size_t count;
	script_message entries[SCRIPT_MESSAGE_LIMIT];
} script_message_queue;

/* Successful admission transfers text ownership; refusal leaves it with the
 * caller. Targets are literal X atom IDs, without a dependency on Xlib. */
bool script_message_enqueue(script_message_queue *queue,
	unsigned long target, char *text);
/* Take the oldest message for this target, transferring text to the caller.
 * An unmatched request leaves every queued message untouched. */
char *script_message_take(script_message_queue *queue, unsigned long target);
void script_message_clear(script_message_queue *queue);

#endif
