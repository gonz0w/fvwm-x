#ifndef FVWMFORM_FORM_INPUT_H
#define FVWMFORM_FORM_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include "libs/module_x_service.h"

#define FORM_X_EVENT_BUDGET MODULE_X_EVENT_BUDGET
#define FORM_X_SERVICE_NS MODULE_X_SERVICE_NS

typedef struct
{
	int ticks;
	const char *command;
	size_t command_length;
	bool quoted_command;
	const char *text;
	bool quoted_text;
} form_timeout_args;

typedef struct
{
	const char *mode;
	const char *text;
	const char *key;
	bool quoted_text;
} form_button_args;

typedef struct
{
	const char *filename;
	const char *prefix;
} form_data_args;

typedef struct
{
	const char *name;
	const char *initial;
	int width;
} form_input_args;

bool form_packet_is_valid(unsigned long type, const unsigned long *body,
	size_t words);
/* One serial per button invocation, shared by its command substitutions. */
bool form_command_begin(void);
/* Append literal text with a finite bound; leave owned storage on refusal. */
bool form_append_text(char **owned, const char *tail, size_t limit);
bool form_chomp_line(char *line);
/* Returned copies are owned by the caller. A final backslash is literal. */
char *form_copy_quoted(const char *text);
char *form_copy_token(const char *text);
/* Borrow views only after all required timeout arguments are present. */
bool form_parse_timeout(const char *text, form_timeout_args *args);
/* Key arguments refer to the source tail, before translation/unescaping. */
bool form_parse_button(const char *text, form_button_args *args);
/* Skip a whitespace-delimited field and borrow its next argument or NUL. */
const char *form_next_argument(const char *text);
bool form_parse_data(const char *text, form_data_args *args);
/* Checked capacity for counted arrays; spare includes the next insertion. */
bool form_array_capacity(int current, int used, int spare, int initial,
	size_t element_size, int *capacity);
bool form_input_width(const char *text, int *width);
bool form_parse_input(const char *text, form_input_args *args);
/* One complete quoted presentation field; at most 1024 source bytes. */
bool form_label_text(const char *text);
bool form_input_geometry(int width, int font_height, int horizontal_padding,
	int vertical_padding, int *size_x, int *size_y);
/* Navigate the existing recall ring; empty storage has no successor. */
bool form_history_position(char *const *values, int slots, int current,
	int direction, int *position);
/* Prepare insertion counts/capacity before changing the owned input value. */
bool form_input_growth(int capacity, int bytes, int characters,
	size_t added_bytes, int added_characters, int *new_capacity,
	int *new_bytes, int *new_characters);
int form_readiness_timeout(bool queued_x, bool timer_pending);
bool form_x_service_available(unsigned int serviced,
	const struct timespec *started, const struct timespec *now);

#endif
