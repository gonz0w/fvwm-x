#ifndef FVWM_IMAGE_SAFETY_H
#define FVWM_IMAGE_SAFETY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool FImageSizeIsSafe(
	uint64_t width, uint64_t height, uint64_t max_dimension,
	size_t max_pixels, size_t *pixel_count);

bool FEwmhIconFindBest(
	const uint32_t *data, size_t data_len, uint32_t wanted_width,
	uint32_t wanted_height, uint32_t max_dimension, size_t max_pixels,
	size_t *pixel_start, uint32_t *width, uint32_t *height);

#endif
