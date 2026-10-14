/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/image_effect_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Test-only fault injection. Normal work execs the real private worker with
 * untouched stdin. Other modes consume the same checked request contract. */
int main(int argc, char **argv)
{
	assert(argc == 3 && strcmp(argv[1], "--effects") == 0);
	const char *control = getenv("FVWM_TEST_EFFECT_CONTROL");
	const char *events = getenv("FVWM_TEST_EFFECT_EVENTS");
	const char *worker = getenv("FVWM_TEST_REAL_EFFECT_WORKER");
	char mode[32];
	FILE *file = fopen(control, "r");

	assert(file != NULL && fgets(mode, sizeof(mode), file) != NULL);
	assert(fclose(file) == 0);
	mode[strcspn(mode, "\n")] = 0;
	file = fopen(events, "a");
	assert(file != NULL);
	assert(fprintf(file, "%ld %s\n", (long)getpid(), mode) > 0);
	assert(fclose(file) == 0);
	if (strcmp(mode, "pass") == 0)
	{
		assert(worker != NULL && worker[0] == '/');
		argv[0] = (char *)worker;
		execv(worker, argv);
		return 127;
	}
	unsigned char bytes[IMAGE_EFFECT_REQUEST_SIZE];
	size_t used = 0;

	while (used < sizeof(bytes))
	{
		ssize_t received = read(STDIN_FILENO, bytes + used,
		    sizeof(bytes) - used);

		assert(received > 0);
		used += received;
	}
	image_effect_request request;

	assert(image_effect_request_decode(bytes, used, &request));
	unsigned char header[IMAGE_EFFECT_HEADER_SIZE];

	image_effect_header(header, request.width, request.height);
	if (strcmp(mode, "partial") == 0)
	{
		assert(write(STDOUT_FILENO, header, 7) == 7);
		return 0;
	}
	if (strcmp(mode, "dimensions") == 0)
	{
		image_effect_header(header, request.width + 1, request.height);
		assert(write(STDOUT_FILENO, header, sizeof(header)) == sizeof(header));
		return 0;
	}
	if (strcmp(mode, "complete-stall") == 0)
	{
		assert(write(STDOUT_FILENO, header, sizeof(header)) == sizeof(header));
		image_effect_pixel pixel = {.red = 65535,
		    .kind = IMAGE_EFFECT_RGB};
		unsigned char record[IMAGE_EFFECT_RECORD_SIZE];

		image_effect_record(record, &pixel);
		for (size_t i = 0; i < (size_t)request.width * request.height; i++)
		{
			assert(write(STDOUT_FILENO, record, sizeof(record)) == sizeof(record));
		}
		assert(close(STDOUT_FILENO) == 0);
	}
	else
	{
		assert(strcmp(mode, "stall") == 0);
	}
	for (;;)
	{
		pause();
	}
}
