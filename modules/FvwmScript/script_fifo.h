#ifndef FVWMSCRIPT_SCRIPT_FIFO_H
#define FVWMSCRIPT_SCRIPT_FIFO_H

#include <stdbool.h>
#include <stddef.h>

#define SCRIPT_FIFO_DEFAULT_TIMEOUT_MS 5000
#define SCRIPT_FIFO_MAX_TIMEOUT_MS 3600000
#define SCRIPT_FIFO_REPLY_MAX_BYTES 32000U

typedef bool (*script_fifo_cancel_fn)(void *context);

/* Existing named-pipe protocol within directory. Modes 0/1 share one elapsed
 * deadline; mode 2 makes one immediate atomic write. Successful mode-1 text is
 * owned and terminated, including an empty reply. Failure clears outputs and
 * preserves errno; it may have sent part of the command. No helper is killed. */
bool script_fifo_exchange(const char *directory, const char *id,
	const char *command, int mode, int timeout_ms,
	script_fifo_cancel_fn cancel, void *context, char **reply, size_t *length);

#endif
