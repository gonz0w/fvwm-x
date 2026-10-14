#ifndef FVWMLIB_FONT_TEXT_H
#define FVWMLIB_FONT_TEXT_H

#include <stddef.h>
#include <stdint.h>

/* Decode one bounded UTF-8 scalar. Malformed bytes advance individually;
 * callers projecting to a core X font replace scalars outside its BMP. */
static inline size_t font_text_utf8(const unsigned char *text, size_t length,
	uint32_t *scalar)
{
	unsigned count;
	uint32_t value, minimum;

	*scalar = 0xfffd;
	if (length == 0)
		return 0;
	if (text[0] < 0x80)
	{
		*scalar = text[0];
		return 1;
	}
	if (text[0] >= 0xc2 && text[0] <= 0xdf)
	{
		count = 2; value = text[0] & 0x1f; minimum = 0x80;
	}
	else if (text[0] >= 0xe0 && text[0] <= 0xef)
	{
		count = 3; value = text[0] & 0x0f; minimum = 0x800;
	}
	else if (text[0] >= 0xf0 && text[0] <= 0xf4)
	{
		count = 4; value = text[0] & 7; minimum = 0x10000;
	}
	else
		return 1;
	if (length < count)
		return 1;
	for (unsigned i = 1; i < count; i++)
	{
		if ((text[i] & 0xc0) != 0x80)
			return 1;
		value = (value << 6) | (text[i] & 0x3f);
	}
	if (value < minimum || value > 0x10ffff ||
	    (value >= 0xd800 && value <= 0xdfff))
		return 1;
	*scalar = value;
	return count;
}

#endif
