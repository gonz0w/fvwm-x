#ifndef FVWM_BUTTONS_STARTUP_H
#define FVWM_BUTTONS_STARTUP_H

#include "libs/defaults.h"
#include <stdbool.h>

typedef struct
{
	char command[MAX_MODULE_INPUT_TEXT_LEN + 1];
	bool consumed;
} buttons_startup_state;

/* Last valid value wins before consumption. Remove one physical LF/CRLF
 * terminator, trim horizontal edge whitespace, reject remaining CR/LF and
 * empty/oversized commands. The cap excludes that physical terminator.
 * Failure preserves state. */
bool buttons_startup_set(buttons_startup_state *state, const char *text);
/* Consume even an unset action. A returned command borrows state's storage;
 * the caller sends once, then clears that storage. No replay or late set. */
const char *buttons_startup_take(buttons_startup_state *state);

#endif
