/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/image_svg.h"
#include "libs/image_svg_protocol.h"
#include "libs/image_effect_x.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_RSVG
static bool
worker_unsigned(const char *text, unsigned *number)
{
	char *end;
	unsigned long value;

	if (text == NULL || text[0] < '0' || text[0] > '9')
	{
		return false;
	}
	errno = 0;
	value = strtoul(text, &end, 10);
	if (errno != 0 || *end != '\0' || value > UINT_MAX)
	{
		return false;
	}
	*number = (unsigned)value;
	return true;
}

static bool
worker_source(image_input *source)
{
	size_t capacity = 0;
	unsigned char bytes[65536];

	for (;;)
	{
		size_t room = (size_t)FVWM_IMAGE_INPUT_LIMIT - source->length;
		size_t ask = room < sizeof(bytes) ? room + 1 : sizeof(bytes);
		ssize_t count = read(STDIN_FILENO, bytes, ask);

		if (count < 0 && errno == EINTR)
		{
			continue;
		}
		if (count < 0 || (size_t)count > room)
		{
			return false;
		}
		if (count == 0)
		{
			return source->length != 0;
		}
		size_t needed = source->length + (size_t)count;

		if (needed > capacity)
		{
			size_t next = capacity == 0 ? sizeof(bytes) : capacity;

			while (next < needed)
			{
				next = next > FVWM_IMAGE_INPUT_LIMIT / 2 ?
				    FVWM_IMAGE_INPUT_LIMIT : next * 2;
			}
			unsigned char *grown = realloc(source->bytes, next);

			if (grown == NULL)
			{
				return false;
			}
			capacity = next;
			source->bytes = grown;
		}
		memcpy(source->bytes + source->length, bytes, (size_t)count);
		source->length = needed;
	}
}

#endif

static bool
worker_write(const void *data, size_t length)
{
	const unsigned char *bytes = data;

	while (length != 0)
	{
		ssize_t count = write(STDOUT_FILENO, bytes, length);

		if (count < 0 && errno == EINTR)
		{
			continue;
		}
		if (count <= 0)
		{
			return false;
		}
		bytes += count;
		length -= (size_t)count;
	}
	return true;
}

static bool
worker_effects(const char *display)
{
	unsigned char bytes[IMAGE_EFFECT_REQUEST_SIZE];
	size_t received = 0;
	image_effect_request request;

	while (received < sizeof(bytes))
	{
		ssize_t count = read(STDIN_FILENO, bytes + received,
		    sizeof(bytes) - received);

		if (count < 0 && errno == EINTR)
		{
			continue;
		}
		if (count <= 0)
		{
			return false;
		}
		received += count;
	}
	unsigned char extra;
	ssize_t count;

	do
	{
		count = read(STDIN_FILENO, &extra, 1);
	} while (count < 0 && errno == EINTR);
	return count == 0 && image_effect_request_decode(bytes, sizeof(bytes),
	    &request) && image_effect_render(display, &request, worker_write);
}

int
main(int argc, char **argv)
{
	if (argc == 3 && strcmp(argv[1], "--effects") == 0)
	{
		return worker_effects(argv[2]) ? 0 : 1;
	}

#ifdef HAVE_RSVG
	image_input source = {0};
	monitor_scale_factor factor;
	unsigned target_width, target_height;
	uint32_t *pixels = NULL;
	int width = 0, height = 0;
	unsigned char header[IMAGE_SVG_HEADER_SIZE];
	int status = 1;

	if (argc != 6 || strcmp(argv[1], "--stdio") != 0 ||
	    !worker_unsigned(argv[3], &factor.quarters) ||
	    factor.quarters < MONITOR_SCALE_QUARTERS_MIN ||
	    factor.quarters > MONITOR_SCALE_QUARTERS_MAX ||
	    !worker_unsigned(argv[4], &target_width) ||
	    !worker_unsigned(argv[5], &target_height))
	{
		fprintf(stderr, "Private FVWM SVG decoder; invoke through FVWM.\n");
		return 2;
	}
	if (!worker_source(&source) ||
	    !image_svg_render(&source, argv[2], factor, target_width,
	    target_height, &pixels, &width, &height))
	{
		goto done;
	}
	image_svg_header(header, (unsigned)width, (unsigned)height);
	if (worker_write(header, sizeof(header)) &&
	    worker_write(pixels, (size_t)width * (size_t)height * sizeof(*pixels)))
	{
		status = 0;
	}
done:
	free(pixels);
	free(source.bytes);
	return status;
#else
	fprintf(stderr, "Private FVWM image worker; invoke through FVWM.\n");
	return 2;
#endif
}
