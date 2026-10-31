#ifndef FVWM_IMAGE_BACKGROUND_H
#define FVWM_IMAGE_BACKGROUND_H
#include "image_process.h"
#include "image_svg_protocol.h"
#include "background_plan.h"

/* Private worker: literal input opens and all PNG/cover loops stay isolated. */
bool image_background_render(const struct background_plan *plan,
	unsigned width, unsigned height, uint32_t **pixels);
image_process *image_background_start(const struct background_plan *plan,
	unsigned width, unsigned height, image_input *transport,
	image_svg_result *result);
#endif
