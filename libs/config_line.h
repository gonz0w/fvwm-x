#ifndef FVWM_CONFIG_LINE_H
#define FVWM_CONFIG_LINE_H

#include <stddef.h>
#include <stdio.h>

#define FVWM_CONFIG_LINE_MAX_BYTES (1024U * 1024U)

/* Owned reusable storage; returns 1 for a complete logical line, 0 at EOF and
 * -1 for I/O, embedded NUL or a line beyond the finite bound. No partial line
 * may be dispatched after refusal. Backslash-newline preserves continuation. */
int fvwm_config_line(FILE *stream, char **line, size_t *capacity);

#endif
