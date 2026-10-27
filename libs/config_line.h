#ifndef FVWM_CONFIG_LINE_H
#define FVWM_CONFIG_LINE_H

#include <stddef.h>
#include <stdio.h>

#define FVWM_CONFIG_LINE_MAX_BYTES (1024U * 1024U)

/* Owned reusable storage; returns 1 for a complete logical line, 0 at EOF and
 * -1 for I/O, embedded NUL or a line beyond the finite bound. No partial line
 * may be dispatched after refusal. Backslash-newline preserves continuation. The byte bound also
 * covers consumed physical bytes, including continuation/newline bytes. */
int fvwm_config_line(FILE *stream, char **line, size_t *capacity);

/* next_line starts at 1; outputs are zero on EOF/refusal. Physical lines
 * include each consumed continuation and the final unterminated line. */
int fvwm_config_line_span(FILE *stream, char **line, size_t *capacity,
	size_t *next_line, size_t *first, size_t *last);

#endif
