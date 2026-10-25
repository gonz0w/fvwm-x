#ifndef FVWMSCRIPT_SCRIPT_PROCESS_H
#define FVWMSCRIPT_SCRIPT_PROCESS_H

#include <stdbool.h>
#include <stddef.h>

#define SCRIPT_PROCESS_DEFAULT_TIMEOUT_MS 5000
#define SCRIPT_PROCESS_MAX_TIMEOUT_MS 3600000

/* Trusted script shell command; not a sandbox. Successful text is caller-owned.
 * Failure clears outputs. The deadline includes output and command completion;
 * Timeout or module termination discards partial output and terminates the
 * command's process group. Cancellation reports ECANCELED; child setup/exec
 * failure reports its errno, independently of an executed shell's exit status. */
bool script_process_read(const char *command, int timeout_ms, char **text,
	size_t *length);

#endif
