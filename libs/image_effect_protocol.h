#ifndef FVWMLIB_IMAGE_EFFECT_PROTOCOL_H
#define FVWMLIB_IMAGE_EFFECT_PROTOCOL_H

#include "image_effect.h"
#include "image_svg_protocol.h"

#define IMAGE_EFFECT_HEADER_SIZE 16U
#define IMAGE_EFFECT_REQUEST_SIZE 60U
#define IMAGE_EFFECT_RECORD_SIZE 8U
#define IMAGE_EFFECT_MAX_OUTPUT (IMAGE_EFFECT_HEADER_SIZE + \
	(size_t)IMAGE_SVG_MAX_PIXELS * IMAGE_EFFECT_RECORD_SIZE)

typedef struct
{
	uint32_t width, height;
	uint32_t pixmap, mask, alpha;
	uint32_t visual, depth, alpha_depth, colormap;
	uint32_t background, tint;
	uint32_t tint_percent, alpha_percent;
} image_effect_request;

typedef struct
{
	unsigned char header[IMAGE_EFFECT_HEADER_SIZE];
	size_t header_used;
	unsigned char partial[IMAGE_EFFECT_RECORD_SIZE];
	size_t partial_used;
	image_effect_pixel *pixels;
	size_t count;
	size_t received;
	unsigned width, height;
	/* Optional expected dimensions, checked before allocating output. */
	unsigned expected_width, expected_height;
	bool failed;
} image_effect_result;

/* Private fixed-width little-endian protocol, independent of host ABI. */
bool image_effect_request_encode(unsigned char bytes[IMAGE_EFFECT_REQUEST_SIZE],
	const image_effect_request *request);
bool image_effect_request_decode(const unsigned char *bytes, size_t length,
	image_effect_request *request);
void image_effect_header(unsigned char bytes[IMAGE_EFFECT_HEADER_SIZE],
	unsigned width, unsigned height);
void image_effect_record(unsigned char bytes[IMAGE_EFFECT_RECORD_SIZE],
	const image_effect_pixel *pixel);
bool image_effect_result_consume(const unsigned char *bytes, size_t length,
	void *context);
bool image_effect_result_complete(const image_effect_result *result);
void image_effect_result_destroy(image_effect_result *result);

#endif
