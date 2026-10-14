/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "image_process.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_DECOMPRESS_CHUNK 65536U

static bool
image_decompress_append(image_input *output, size_t *capacity,
	const unsigned char *bytes, size_t count, size_t limit)
{
	if (count > limit - output->length)
	{
		errno = EFBIG;
		return false;
	}
	if (output->length + count > *capacity || output->bytes == NULL)
	{
		size_t next = *capacity == 0 ?
		    IMAGE_DECOMPRESS_CHUNK : *capacity;

		if (next > limit)
		{
			next = limit;
		}
		while (next < output->length + count)
		{
			next = next > limit / 2 ? limit : next * 2;
		}
		unsigned char *grown = realloc(output->bytes, next + 1);

		if (grown == NULL)
		{
			return false;
		}
		output->bytes = grown;
		*capacity = next;
	}
	if (count > 0)
	{
		memcpy(output->bytes + output->length, bytes, count);
	}
	output->length += count;
	output->bytes[output->length] = '\0';
	return true;
}

static bool
image_decompress_collect(const unsigned char *bytes, size_t length,
	void *context)
{
	image_decompress_result *sink = context;

	return image_decompress_append(&sink->output, &sink->capacity,
	    bytes, length, sink->limit);
}

void
image_decompress_result_destroy(image_decompress_result *result)
{
	if (result != NULL)
	{
		image_input_destroy(&result->output);
		result->capacity = result->limit = 0;
	}
}

image_process *
image_decompress_start(const image_input *source, const char *program,
	char *const argv[], size_t limit, int timeout_ms,
	image_decompress_result *result)
{
	if (result == NULL || result->output.bytes != NULL ||
	    result->output.length != 0 || result->capacity != 0)
	{
		errno = EINVAL;
		return NULL;
	}
	result->limit = limit;
	return image_process_start(source, program, argv, limit, timeout_ms, 0,
	    image_decompress_collect, result);
}

bool
image_decompress(const image_input *source, const char *program,
	char *const argv[], size_t limit, int timeout_ms,
	image_decompress_cancel_fn cancel, void *context, image_input *output)
{
	image_decompress_result sink = {.limit = limit};
	bool success;

	if (output == NULL)
	{
		errno = EINVAL;
		return false;
	}
	output->bytes = NULL;
	output->length = 0;
	success = image_process_run(source, program, argv, limit, timeout_ms, 0,
	    image_decompress_collect, &sink, cancel, context);
	if (success)
	{
		/* Preserve NUL termination even for empty synchronous output. */
		success = image_decompress_collect(NULL, 0, &sink);
	}
	if (success)
	{
		*output = sink.output;
	}
	else
	{
		image_input_destroy(&sink.output);
	}
	return success;
}
