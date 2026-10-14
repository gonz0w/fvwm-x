#ifndef FVWMSCRIPT_SCRIPT_TEXT_H
#define FVWMSCRIPT_SCRIPT_TEXT_H

#include <stdbool.h>
#include <stddef.h>

typedef int (*script_text_step_fn)(const char *text, void *context);

/* The locale callback examines the current character. Clamp its byte stride
 * to the remaining input, including incomplete final multibyte characters. */
size_t
script_text_next(const char *text, size_t remaining, script_text_step_fn step,
    void *context);
int
script_text_byte_offset(const char *text, size_t length, int character,
    script_text_step_fn step, void *context);
int
script_text_char_offset(const char *text, size_t length, int byte,
    script_text_step_fn step, void *context);

/* Replace [first,last) in owned, terminated text. Aliased insertion is safe.
 * Invalid ranges, unrepresentable lengths and allocation refusal keep the
 * original pointer/content. maximum excludes the terminating byte. */
bool
script_text_replace(char **text, size_t first, size_t last, const char *insert,
    size_t insert_length, size_t maximum);
bool
script_text_delete(char *text, size_t first, size_t last);
/* Interactive edits obey ValueLimit, but may shrink an older oversized title.
 * Resulting byte positions always fit the widget's int indices. */
bool
script_text_edit(char **text, size_t first, size_t last, const char *insert,
    size_t insert_length, size_t maximum);
/* Legacy menu options are 1-based; indices <= 1 select the first option.
 * Missing options are empty. The returned span borrows the original text. */
const char *
script_text_option(const char *text, int index, size_t *length);

#endif
