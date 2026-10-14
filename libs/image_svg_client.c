/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "image_svg_client.h"
#include "image_process.h"
#include "ImageSafety.h"
#include "fvwmsignal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
image_svg_cancelled(void *context)
{
	(void)context;
	return isTerminated != 0;
}

typedef struct
{
	const char *program;
	char quarters[32];
	char target_w[32];
	char target_h[32];
	char *args[7];
} image_svg_request;

static bool
image_svg_prepare(const image_input *source, const char *path,
	monitor_scale_factor factor, unsigned target_width,
	unsigned target_height,
	image_svg_request *request)
{
	const char *program = getenv("FVWM_IMAGE_SVG_WORKER");

	if (program == NULL)
	{
		program = FVWM_IMAGE_SVG_WORKER;
	}
	if (source == NULL || (source->length && source->bytes == NULL) ||
	    source->length > FVWM_IMAGE_INPUT_LIMIT ||
	    path == NULL || program[0] != '/' ||
	    factor.quarters < MONITOR_SCALE_QUARTERS_MIN ||
	    factor.quarters > MONITOR_SCALE_QUARTERS_MAX)
	{
		errno = EINVAL;
		return false;
	}
	/* A PNG signature cannot start SVG XML or gzip data. Refuse it here
	 * rather than spawning a decoder for each failed PNG fallback. */
	if (source->bytes != NULL && source->length >= 4 &&
	    memcmp(source->bytes, "\211PNG", 4) == 0)
	{
		errno = EINVAL;
		return false;
	}

	snprintf(request->quarters, sizeof(request->quarters), "%u",
	    factor.quarters);
	snprintf(request->target_w, sizeof(request->target_w), "%u",
	    target_width);
	snprintf(request->target_h, sizeof(request->target_h), "%u",
	    target_height);
	request->program = program;
	request->args[0] = (char *)program;
	request->args[1] = "--stdio";
	request->args[2] = (char *)path;
	request->args[3] = request->quarters;
	request->args[4] = request->target_w;
	request->args[5] = request->target_h;
	request->args[6] = NULL;
	return true;
}

image_process *
image_svg_start(const image_input *source, const char *path,
	monitor_scale_factor factor, unsigned target_width,
	unsigned target_height,
	image_svg_result *result)
{
	image_svg_request request;

	if (result == NULL || result->header_used != 0 ||
	    result->pixels != NULL || result->pixel_bytes != 0 ||
	    result->received != 0 || result->width != 0 || result->height != 0)
	{
		errno = EINVAL;
		return NULL;
	}
	if (!image_svg_prepare(source, path, factor, target_width,
	    target_height,
	    &request))
	{
		return NULL;
	}
	/* argv is consumed by fork before start returns; source/result remain
	 * borrowed by the returned state. */
	return image_process_start(source, request.program, request.args,
	    IMAGE_SVG_MAX_OUTPUT, FVWM_IMAGE_SVG_TIMEOUT_MS,
	    FVWM_IMAGE_SVG_MEMORY_LIMIT, image_svg_result_consume, result);
}

bool
image_svg_load(const image_input *source, const char *path,
	monitor_scale_factor factor, unsigned target_width,
	unsigned target_height,
	uint32_t **pixels, int *width, int *height)
{
	image_svg_result result = {0};
	image_svg_request request;
	bool success;

	if (pixels == NULL || width == NULL || height == NULL)
	{
		errno = EINVAL;
		return false;
	}
	*pixels = NULL;
	*width = 0;
	*height = 0;
	if (!image_svg_prepare(source, path, factor, target_width,
	    target_height,
	    &request))
	{
		return false;
	}
	success = image_process_run(source, request.program, request.args,
	    IMAGE_SVG_MAX_OUTPUT, FVWM_IMAGE_SVG_TIMEOUT_MS,
	    FVWM_IMAGE_SVG_MEMORY_LIMIT, image_svg_result_consume, &result,
	    image_svg_cancelled, NULL);
	if (success && image_svg_result_complete(&result))
	{
		*pixels = result.pixels;
		*width = result.width;
		*height = result.height;
		return true;
	}
	if (success)
	{
		errno = EPROTO;
	}
	image_svg_result_destroy(&result);
	return false;
}
