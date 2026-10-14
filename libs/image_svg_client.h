#ifndef FVWMLIB_IMAGE_SVG_CLIENT_H
#define FVWMLIB_IMAGE_SVG_CLIENT_H

#include "image_input.h"
#include "image_process.h"
#include "image_svg_protocol.h"
#include "monitor_scale.h"
#include <stdint.h>

/* Borrows source/result until the process is reaped. The result must start
 * empty and be destroyed by the caller, including on cancellation/failure.
 * Process success alone is insufficient: check image_svg_result_complete.
 * The caller services ordinary readiness; this entry point never waits. */
image_process *image_svg_start(const image_input *source, const char *path,
	monitor_scale_factor factor, unsigned target_width,
	unsigned target_height,
	image_svg_result *result);

bool image_svg_load(const image_input *source, const char *path,
	monitor_scale_factor factor, unsigned target_width,
	unsigned target_height,
	uint32_t **pixels, int *width, int *height);

#endif
