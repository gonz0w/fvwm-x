/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/*

Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

/* Bounded buffer adaptation of Xlib RdBitF.c. Preserve its X10/X11
 * syntax and row packing; validate dimensions/numbers before allocation.
 * The original pathname is never reopened by the bitmap fallback. */
#include "config.h"
#include "image_xbm.h"
#include "ImageSafety.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_XBM_LINE_BYTES 255

static bool
image_xbm_line(const image_input *source, size_t *position,
	char line[IMAGE_XBM_LINE_BYTES])
{
	size_t used = 0;

	while (*position < source->length && used < IMAGE_XBM_LINE_BYTES - 1)
	{
		unsigned char byte = source->bytes[(*position)++];

		line[used++] = (char)byte;
		if (byte == '\n')
		{
			break;
		}
	}
	line[used] = '\0';
	return used != 0;
}

static bool
image_xbm_hex(const image_input *source, size_t *position,
	unsigned *result)
{
	unsigned value = 0;
	bool found = false;

	while (*position < source->length)
	{
		unsigned char byte = source->bytes[(*position)++];
		unsigned digit;

		if (byte >= '0' && byte <= '9')
		{
			digit = byte - '0';
		}
		else if (byte >= 'a' && byte <= 'f')
		{
			digit = byte - 'a' + 10;
		}
		else if (byte >= 'A' && byte <= 'F')
		{
			digit = byte - 'A' + 10;
		}
		else
		{
			if (found && (byte == ' ' || byte == ',' ||
			    byte == '}' ||
			    byte == '\n' || byte == '\t'))
			{
				*result = value;
				return true;
			}
			continue;
		}
		if (value > ((unsigned)INT_MAX - digit) / 16)
		{
			return false;
		}
		value = value * 16 + digit;
		found = true;
	}
	return false;
}

bool
image_xbm_read(const image_input *source, unsigned max_dimension,
	size_t max_pixels, unsigned char **bits, unsigned *width,
	unsigned *height, int *x_hot, int *y_hot)
{
	char line[IMAGE_XBM_LINE_BYTES], name[IMAGE_XBM_LINE_BYTES];
	unsigned w = 0, h = 0;
	int hx = -1, hy = -1;
	size_t position = 0, pixels;

	*bits = NULL;
	*width = *height = 0;
	*x_hot = *y_hot = -1;
	if (source == NULL || (source->length > 0 && source->bytes == NULL))
	{
		return false;
	}
	while (image_xbm_line(source, &position, line))
	{
		int offset = 0, old_format;
		char *type;

		if (strlen(line) == IMAGE_XBM_LINE_BYTES - 1)
		{
			return false;
		}
		if (sscanf(line, "#define %254s %n", name, &offset) == 1 &&
		    offset > 0)
		{
			char *end;
			long number;

			type = strrchr(name, '_');
			type = type != NULL ? type + 1 : name;
			if (strcmp(type, "width") != 0 &&
			    strcmp(type, "height") != 0 &&
			    !(strcmp(type, "hot") == 0 && type - name >= 2 &&
			      (strcmp(type - 2, "x_hot") == 0 ||
			       strcmp(type - 2, "y_hot") == 0)))
			{
				continue;
			}
			errno = 0;
			number = strtol(line + offset, &end, 10);
			if (end == line + offset)
			{
				continue;
			}
			if (errno == ERANGE || number < INT_MIN ||
			    number > INT_MAX)
			{
				return false;
			}
			if (strcmp(type, "width") == 0)
			{
				w = (unsigned)number;
			}
			if (strcmp(type, "height") == 0)
			{
				h = (unsigned)number;
			}
			if (strcmp(type, "hot") == 0 && type - name >= 2)
			{
				if (strcmp(type - 2, "x_hot") == 0)
				{
					hx = (int)number;
				}
				if (strcmp(type - 2, "y_hot") == 0)
				{
					hy = (int)number;
				}
			}
			continue;
		}
		if (sscanf(line, "static short %254s = {", name) == 1)
		{
			old_format = 1;
		}
		else if (sscanf(line,
		    "static unsigned char %254s = {", name) == 1 ||
		    sscanf(line, "static char %254s = {", name) == 1)
		{
			old_format = 0;
		}
		else
		{
			continue;
		}
		type = strrchr(name, '_');
		type = type != NULL ? type + 1 : name;
		if (strcmp(type, "bits[]") != 0)
		{
			continue;
		}
		if (!FImageSizeIsSafe(w, h, max_dimension, max_pixels, &pixels))
		{
			return false;
		}
		size_t row_bytes = (size_t)(w / 8) + (w % 8 != 0);
		size_t words = (size_t)(w / 16) + (w % 16 != 0);
		size_t columns = old_format ? words : row_bytes;

		/* Each value needs a hex digit and terminating delimiter.
		 * Bound allocation by checked input for legacy wide bitmaps. */
		if (columns > (source->length - position) / 2 / h)
		{
			return false;
		}
		unsigned char *data = malloc(row_bytes * h);

		if (data == NULL)
		{
			return false;
		}
		for (unsigned row = 0; row < h; row++)
		{
			for (size_t column = 0; column < columns; column++)
			{
				unsigned value;

				if (!image_xbm_hex(source, &position, &value))
				{
					free(data);
					return false;
				}
				if (!old_format)
				{
					data[row * row_bytes + column] = value;
				}
				else
				{
					size_t byte = column * 2;

					data[row * row_bytes + byte] = value;
					if (byte + 1 < row_bytes)
					{
						data[row * row_bytes +
						    byte + 1] =
							value >> 8;
					}
				}
			}
		}
		*bits = data;
		*width = w;
		*height = h;
		*x_hot = hx;
		*y_hot = hy;
		return true;
	}
	return false;
}
