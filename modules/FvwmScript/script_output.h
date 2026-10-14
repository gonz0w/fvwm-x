#ifndef FVWM_SCRIPT_OUTPUT_H
#define FVWM_SCRIPT_OUTPUT_H

#include <stddef.h>

#define SCRIPT_OUTPUT_MAX_BYTES 32000U
#define SCRIPT_OUTPUT_COMMAND_MAX_BYTES (64U * 1024U)

/* Copy a line/word from explicit-length text into owned terminated storage.
 * Zero/nonpositive positions select the first line/word as legacy scripts do;
 * word -1 selects the whole line. Missing positions return empty text. */
char *script_output_extract(const char *text, size_t length, int line,
	int word);

/* Same extraction, but refuse a result larger than limit before allocating.
 * Refusal returns NULL with errno EFBIG. */
char *script_output_extract_limited(const char *text, size_t length, int line,
	int word, size_t limit);

#endif
