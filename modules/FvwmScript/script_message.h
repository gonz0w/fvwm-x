#ifndef FVWMSCRIPT_SCRIPT_MESSAGE_H
#define FVWMSCRIPT_SCRIPT_MESSAGE_H

#include <stdbool.h>
#include <stddef.h>

#define SCRIPT_MESSAGE_LIMIT 40
#define SCRIPT_MESSAGE_MAX_BYTES (32U * 1024U)

typedef struct
{
	unsigned long target;
	char *text;
	size_t length;               /* Includes the wire NUL. */
} script_message;

typedef struct
{
	size_t count;
	bool retired;               /* Loss forbids new unowned admissions. */
	script_message entries[SCRIPT_MESSAGE_LIMIT];
} script_message_queue;

/* Successful admission transfers text ownership; refusal leaves it with the
 * caller. Targets are literal X atom IDs, without a dependency on Xlib. */
bool script_message_enqueue(script_message_queue *queue,
	unsigned long target, char *text);
/* Take the oldest message for this target, transferring text to the caller.
 * An unmatched request leaves every queued message untouched. */
char *script_message_take(script_message_queue *queue, unsigned long target);
/* Inspection never consumes or changes FIFO order. Returned entries are
 * borrowed until the next queue mutation. Targets are unique, in FIFO order. */
const script_message *script_message_peek(const script_message_queue *queue,
	unsigned long target);
size_t script_message_targets(const script_message_queue *queue,
	unsigned long *targets, size_t capacity);
void script_message_clear(script_message_queue *queue);
void script_message_retire(script_message_queue *queue);
/* Only a confirmed new owner may reopen a retired queue. */
void script_message_open(script_message_queue *queue);

#endif
