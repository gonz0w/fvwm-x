#ifndef FVWM_SCRIPT_COMMAND_H
#define FVWM_SCRIPT_COMMAND_H

#include <stdbool.h>
#include <stddef.h>

#define SCRIPT_COMMAND_MAX_BYTES 997U

/* Append an evaluated value once. On rejection, preserve the owned buffer
 * and length. Allocation failures use the module's fatal allocator. */
bool script_text_append(char **text, size_t *length, const char *value,
	size_t limit);

/* Preserve the legacy first-whitespace-delimited filename and argument tail.
 * Returns an owned command or NULL for empty/oversized input. */
char *script_launch_command(const char *module, const char *argument,
	const char *selection);

#endif
