/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/image_svg_protocol.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	assert(argc == 6 && strcmp(argv[1], "--stdio") == 0);
	unsigned char input[4096];
	ssize_t length;
	unsigned char first = 0;

	while ((length = read(STDIN_FILENO, input, sizeof(input))) > 0)
	{
		if (first == 0)
		{
			first = input[0];
		}
	}
	assert(length == 0);
	if (strstr(argv[2], "stall.svg") != NULL)
	{
		for (;;)
		{
			pause();
		}
	}
	if (first == '/' || first == '#')
	{
		/* Real SVG rejects these XPM/bitmap source fixtures. */
		return 23;
	}
	struct timespec delay = {0, 100000000};

	nanosleep(&delay, NULL);
	unsigned width = 2 * strtoul(argv[3], NULL, 10) / 4;
	unsigned height = width;
	if (strstr(argv[2], "large.svg") != NULL)
	{
		width = 4096;
	}
	if (strstr(argv[2], "wide.svg") != NULL)
	{
		width = 4;
	}
	unsigned target = strtoul(argv[4], NULL, 10);

	if (target != 0)
	{
		width = target;
	}
	target = strtoul(argv[5], NULL, 10);
	if (target != 0)
	{
		height = target;
	}
	if (strstr(argv[2], "server-refuse.svg") != NULL)
	{
		width = 32768;
		height = 1;
	}
	assert(width <= 32768 && height <= 224);
	unsigned char header[IMAGE_SVG_HEADER_SIZE];

	image_svg_header(header, width, height);
	if (strstr(argv[2], "incomplete.svg") != NULL)
	{
		assert(write(STDOUT_FILENO, header, 7) == 7);
		return 0;
	}
	assert(write(STDOUT_FILENO, header, sizeof(header)) == sizeof(header));
	uint32_t pixel = first == 'e' ? 0 :
	    first == 'h' ? 0x8080c040 :
	    first == 'r' ? 0xffff0000 : 0xff0000ff;

	for (unsigned i = 0; i < width * height; i++)
	{
		uint32_t output = first == 't' && i == 0 ? 0 : pixel;

		assert(write(STDOUT_FILENO, &output, sizeof(output)) ==
		    sizeof(output));
	}
	return 0;
}
