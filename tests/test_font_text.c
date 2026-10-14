/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/font_text.h"
#include <assert.h>

int main(void)
{
	for (uint32_t value = 0; value <= 0x10ffff; value++)
	{
		unsigned char bytes[4];
		size_t length;
		uint32_t decoded;

		if (value >= 0xd800 && value <= 0xdfff)
			continue;
		if (value < 0x80)
		{
			bytes[0] = value;
			length = 1;
		}
		else if (value < 0x800)
		{
			bytes[0] = 0xc0 | (value >> 6);
			bytes[1] = 0x80 | (value & 0x3f);
			length = 2;
		}
		else if (value < 0x10000)
		{
			bytes[0] = 0xe0 | (value >> 12);
			bytes[1] = 0x80 | ((value >> 6) & 0x3f);
			bytes[2] = 0x80 | (value & 0x3f);
			length = 3;
		}
		else
		{
			bytes[0] = 0xf0 | (value >> 18);
			bytes[1] = 0x80 | ((value >> 12) & 0x3f);
			bytes[2] = 0x80 | ((value >> 6) & 0x3f);
			bytes[3] = 0x80 | (value & 0x3f);
			length = 4;
		}
		assert(font_text_utf8(bytes, length, &decoded) == length);
		assert(decoded == value);
		for (size_t prefix = 1; prefix < length; prefix++)
		{
			assert(font_text_utf8(bytes, prefix, &decoded) == 1);
			assert(decoded == 0xfffd);
		}
	}
	{
		const unsigned char bad[][4] = {
			{0x80}, {0xc0, 0x80}, {0xc1, 0xbf}, {0xc2, 0x20},
			{0xe0, 0x80, 0x80}, {0xed, 0xa0, 0x80},
			{0xf0, 0x80, 0x80, 0x80}, {0xf4, 0x90, 0x80, 0x80},
			{0xf5, 0x80, 0x80, 0x80}, {0xff}};
		uint32_t scalar;
		for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
		{
			assert(font_text_utf8(bad[i], 4, &scalar) == 1);
			assert(scalar == 0xfffd);
		}
		assert(font_text_utf8(NULL, 0, &scalar) == 0);
	}
	return 0;
}
