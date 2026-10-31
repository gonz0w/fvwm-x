/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "image_background.h"
#include "image_png.h"
#include "ImageSafety.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

image_process *image_background_start(const struct background_plan *plan,
	unsigned width, unsigned height, image_input *transport,
	image_svg_result *result)
{
	const char *program = getenv("FVWM_IMAGE_SVG_WORKER");
	char encoded[BACKGROUND_SUFFIX_MAX + 1], w[16], h[16];

	if (!program)
		program = FVWM_IMAGE_SVG_WORKER;
	if (*program != '/' || !background_plan_format(encoded, sizeof(encoded),
		plan))
	{
		errno = EINVAL;
		return NULL;
	}
	snprintf(w, sizeof(w), "%u", width);
	snprintf(h, sizeof(h), "%u", height);
	char *args[] = {(char *)program, "--background", encoded, w, h, NULL};

	return image_process_start(transport, program, args,
		IMAGE_SVG_MAX_OUTPUT, 5000, FVWM_IMAGE_SVG_MEMORY_LIMIT,
		image_svg_result_consume, result);
}
