#ifndef FVWM_IMAGE_PNG_H
#define FVWM_IMAGE_PNG_H
#include "image_input.h"
#include <stdint.h>
bool image_png_decode(const image_input *input, uint32_t **pixels,
	int *width, int *height);
#endif
