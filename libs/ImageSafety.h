#ifndef FVWM_IMAGE_SAFETY_H
#define FVWM_IMAGE_SAFETY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool FImageSizeIsSafe(
	uint64_t width, uint64_t height, uint64_t max_dimension,
	size_t max_pixels, size_t *pixel_count);

typedef enum
{
	F_EWMH_ICON_INVALID = -1,
	F_EWMH_ICON_NOT_FOUND,
	F_EWMH_ICON_FOUND
} FEwmhIconResult;

#define F_EWMH_ICON_MAX_DIMENSION 4096U
#define F_EWMH_ICON_MAX_PIXELS (16U * 1024U * 1024U)

/* Bound a donated image plus a retained packed record. The returned word and
 * byte counts also fit the core's int-sized property interface. */
bool FEwmhIconSizeIsSafe(uint64_t width, uint64_t height,
	size_t retained_words, size_t max_bytes, size_t *pixels, size_t *words);

/* Return the first complete exact record, including its two header words.
 * A match ends the search; an unconsumed suffix is not part of that record. */
FEwmhIconResult FEwmhIconFindExact(
	const uint32_t *data, size_t data_len, uint32_t wanted_width,
	uint32_t wanted_height, size_t *record_start, size_t *record_words);

bool FEwmhIconFindBest(
	const uint32_t *data, size_t data_len, uint32_t wanted_width,
	uint32_t wanted_height, uint32_t max_dimension, size_t max_pixels,
	size_t *pixel_start, uint32_t *width, uint32_t *height);

#endif
