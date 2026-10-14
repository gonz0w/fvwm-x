#ifndef FVWMLIB_IMAGE_SVG_PROTOCOL_H
#define FVWMLIB_IMAGE_SVG_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IMAGE_SVG_HEADER_SIZE 16U
#define IMAGE_SVG_MAX_PIXELS (64U * 1024U * 1024U)
#define IMAGE_SVG_MAX_DIMENSION 32768U
#define IMAGE_SVG_MAX_OUTPUT (IMAGE_SVG_HEADER_SIZE + \
	(size_t)IMAGE_SVG_MAX_PIXELS * sizeof(uint32_t))

typedef struct
{
	unsigned char header[IMAGE_SVG_HEADER_SIZE];
	size_t header_used;
	uint32_t *pixels;
	size_t pixel_bytes;
	size_t received;
	int width;
	int height;
} image_svg_result;

/* Incremental private protocol validation; pixels are allocated only after a
 * complete valid header. The local worker uses the caller's native pixel ABI. */
bool image_svg_result_consume(const unsigned char *bytes, size_t length,
	void *context);
bool image_svg_result_complete(const image_svg_result *result);
void image_svg_result_destroy(image_svg_result *result);
void image_svg_header(unsigned char header[IMAGE_SVG_HEADER_SIZE],
	unsigned width, unsigned height);

#endif
