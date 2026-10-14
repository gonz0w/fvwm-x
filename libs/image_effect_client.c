/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "image_effect_client.h"

#include <errno.h>
#include <stdlib.h>

image_process *image_effect_start(const char *display,
	const image_effect_request *request, image_input *input,
	image_effect_result *result)
{
	const char *program = getenv("FVWM_IMAGE_EFFECT_WORKER");

	if (program == NULL)
	{
		program = FVWM_IMAGE_SVG_WORKER;
	}
	if (display == NULL || program[0] != '/' || input == NULL ||
	    input->bytes != NULL || input->length != 0 || result == NULL ||
	    result->header_used != 0 || result->pixels != NULL ||
	    result->failed || result->received != 0 || result->partial_used != 0)
	{
		errno = EINVAL;
		return NULL;
	}
	unsigned char bytes[IMAGE_EFFECT_REQUEST_SIZE];

	if (!image_effect_request_encode(bytes, request))
	{
		return NULL;
	}
	input->bytes = malloc(sizeof(bytes));
	if (input->bytes == NULL)
	{
		return NULL;
	}
	for (unsigned i = 0; i < sizeof(bytes); i++)
	{
		input->bytes[i] = bytes[i];
	}
	input->length = sizeof(bytes);
	result->expected_width = request->width;
	result->expected_height = request->height;
	char *args[] = {(char *)program, "--effects", (char *)display, NULL};
	image_process *process = image_process_start(input, program, args,
	    IMAGE_EFFECT_MAX_OUTPUT, FVWM_IMAGE_EFFECT_TIMEOUT_MS,
	    FVWM_IMAGE_EFFECT_MEMORY_LIMIT, image_effect_result_consume, result);

	if (process == NULL)
	{
		image_input_destroy(input);
	}
	return process;
}
