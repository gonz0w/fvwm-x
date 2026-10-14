#ifndef FVWMSCRIPT_SCRIPT_VALUE_H
#define FVWMSCRIPT_SCRIPT_VALUE_H

#include <stdbool.h>

#define SCRIPT_VALUE_HEX_MAX_WIDTH 65536

typedef enum
{
	SCRIPT_VALUE_ADD,
	SCRIPT_VALUE_MULTIPLY,
	SCRIPT_VALUE_DIVIDE,
	SCRIPT_VALUE_REMAINDER
} script_value_operation;

/* Preserve atoi's numeric prefix and no-digits-as-zero behavior. Overflow
 * fails with a cleared output; trailing text remains accepted. */
bool script_value_integer(const char *text, int *value);
/* Private long-sized argument tags must not overlap variables/functions.
 * Supported AMD64/ARM64 platforms carry the complete int32 range. */
long script_value_encode_integer(int value);
bool script_value_decode_integer(long encoded, int *value);
bool script_value_calculate(script_value_operation operation, int left,
	int right, int *value);
bool script_value_hex_number(const char *text, int *value);

/* Caller owns successful strings. NULL reports invalid/oversized formatting
 * or malformed framing. Missing substring/record positions return empty text.
 * Parse validates the complete four-decimal-digit record sequence. */
char *script_value_hex(int value, int width);
char *script_value_copy(const char *text, int first, int last);
char *script_value_record(const char *text, int index);

#endif
