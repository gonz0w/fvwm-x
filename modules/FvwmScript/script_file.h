#ifndef FVWM_SCRIPT_FILE_H
#define FVWM_SCRIPT_FILE_H

#include <stddef.h>
#include <sys/types.h>

#define SCRIPT_FILE_MAX_BYTES (1024U * 1024U)
#define SCRIPT_TEXT_MAX_BYTES (64U * 1024U)

/* Owned terminated output, or NULL with errno set. Input has an explicit
 * length; malformed sections and embedded NUL bytes are rejected. */
char *script_file_build(const char *input, size_t length, const char *name,
	pid_t pid, const char *text);

/* Update an owned, single-link regular file without following symlinks.
 * Validate the complete bounded replacement before the first write. */
int script_file_update(const char *path, const char *name, pid_t pid,
	const char *text);

#endif
