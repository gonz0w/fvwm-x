#ifndef FVWM_JSON_PRINT_POLICY_H
#define FVWM_JSON_PRINT_POLICY_H

#include <limits.h>
#include <stddef.h>

/* The printer's existing INT_MAX ceiling also bounds expansion and offsets. */
static inline int json_print_size_add(size_t size, size_t extra, size_t *result)
{
	if (size > INT_MAX || extra > (size_t)INT_MAX - size)
	{
		return 0;
	}
	*result = size + extra;
	return 1;
}

/* Return the valid sequence length, or zero for this invalid byte. Look ahead
 * only after a nonzero continuation: even truncated C strings are safe.
 */
static inline size_t json_utf8_sequence_length(const unsigned char *text)
{
	unsigned char lead = text[0];
	if (lead < 0x80)
	{
		return 1;
	}
	if (lead < 0xc2 || lead > 0xf4)
	{
		return 0;
	}
	unsigned char second = text[1];
	if (second < 0x80 || second > 0xbf ||
	    (lead == 0xe0 && second < 0xa0) ||
	    (lead == 0xed && second > 0x9f) ||
	    (lead == 0xf0 && second < 0x90) ||
	    (lead == 0xf4 && second > 0x8f))
	{
		return 0;
	}
	if (lead < 0xe0)
	{
		return 2;
	}
	if (text[2] < 0x80 || text[2] > 0xbf)
	{
		return 0;
	}
	if (lead < 0xf0)
	{
		return 3;
	}
	return text[3] >= 0x80 && text[3] <= 0xbf ? 4 : 0;
}

#endif
