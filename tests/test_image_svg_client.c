/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/image_svg_client.h"
#include "libs/image_process.h"
#include "libs/fvwmsignal.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

volatile sig_atomic_t isTerminated;
static unsigned starts;
static int mode;
static const char literal[] = ":2x2:/tmp/a name;$(literal).svg";

bool
image_process_run(const image_input *source, const char *program,
	char *const argv[], size_t limit, int timeout_ms, uint64_t memory_limit,
	image_process_sink_fn sink, void *sink_context,
	image_decompress_cancel_fn cancel, void *cancel_context)
{
	unsigned char bytes[IMAGE_SVG_HEADER_SIZE + 4];
	uint32_t red = 0xffff0000U;

	starts++;
	assert(source != NULL && strcmp(program, "/tmp/trusted-worker") == 0);
	assert(strcmp(argv[0], program) == 0);
	assert(strcmp(argv[1], "--stdio") == 0);
	assert(strcmp(argv[2], literal) == 0);
	assert(strcmp(argv[3], "4") == 0 && strcmp(argv[4], "0") == 0);
	assert(strcmp(argv[5], "0") == 0 && argv[6] == NULL);
	assert(limit == IMAGE_SVG_MAX_OUTPUT);
	assert(timeout_ms == FVWM_IMAGE_SVG_TIMEOUT_MS);
	assert(memory_limit == FVWM_IMAGE_SVG_MEMORY_LIMIT);
	if (cancel != NULL)
	{
		assert(cancel(cancel_context) == (isTerminated != 0));
	}
	if (mode == 2)
	{
		errno = ENOMEM;
		return false;
	}
	image_svg_header(bytes, 1, 1);
	memcpy(bytes + IMAGE_SVG_HEADER_SIZE, &red, sizeof(red));
	assert(sink(bytes, mode == 1 ? 7 : sizeof(bytes), sink_context));
	return true;
}

image_process *
image_process_start(const image_input *source, const char *program,
	char *const argv[], size_t limit, int timeout_ms, uint64_t memory_limit,
	image_process_sink_fn sink, void *sink_context)
{
	/* Reuse literal argument and protocol checks for both entry points. */
	return image_process_run(source, program, argv, limit, timeout_ms,
	    memory_limit, sink, sink_context, NULL, NULL) ?
	    (image_process *)&starts : NULL;
}

static void
check(const unsigned char *bytes, size_t length, unsigned quarters,
	bool success, unsigned expected_starts)
{
	image_input source = {(unsigned char *)bytes, length};
	uint32_t *pixels = (uint32_t *)1;
	int width = -1, height = -1;

	starts = 0;
	assert(image_svg_load(&source, literal, (monitor_scale_factor){quarters},
	    0, 0, &pixels, &width, &height) == success);
	assert(starts == expected_starts);
	if (success)
	{
		assert(width == 1 && height == 1 && pixels[0] == 0xffff0000U);
		free(pixels);
	}
	else
	{
		assert(pixels == NULL && width == 0 && height == 0);
	}
}

static void
check_async(const unsigned char *bytes, size_t length, unsigned quarters,
	bool started, bool complete, unsigned expected_starts)
{
	image_input source = {(unsigned char *)bytes, length};
	image_svg_result result = {0};

	starts = 0;
	image_process *process = image_svg_start(&source, literal,
	    (monitor_scale_factor){quarters}, 0, 0, &result);

	assert((process != NULL) == started);
	assert(starts == expected_starts);
	assert(image_svg_result_complete(&result) == complete);
	if (complete)
	{
		assert(result.width == 1 && result.height == 1);
		assert(result.pixels[0] == 0xffff0000U);
	}
	image_svg_result_destroy(&result);
}

int
main(void)
{
	const unsigned char xml[] = "<svg/>";
	const unsigned char png[] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
	const unsigned char gzip[] = {0x1f, 0x8b, 0x08, 0x00};
	const unsigned char utf16[] = {0xff, 0xfe, '<', 0, 's', 0};

	assert(setenv("FVWM_IMAGE_SVG_WORKER", "/tmp/trusted-worker", 1) == 0);
	check(png, sizeof(png), 4, false, 0);
	check(png, 4, 4, false, 0);
	check(png, 3, 4, true, 1);
	check(xml, sizeof(xml) - 1, 4, true, 1);
	check(gzip, sizeof(gzip), 4, true, 1);
	check(utf16, sizeof(utf16), 4, true, 1);
	check(xml, sizeof(xml) - 1, 3, false, 0);
	assert(setenv("FVWM_IMAGE_SVG_WORKER", "relative-worker", 1) == 0);
	check(xml, sizeof(xml) - 1, 4, false, 0);
	assert(setenv("FVWM_IMAGE_SVG_WORKER", "/tmp/trusted-worker", 1) == 0);
	check_async(xml, sizeof(xml) - 1, 4, true, true, 1);
	check_async(png, sizeof(png), 4, false, false, 0);
	check_async(xml, sizeof(xml) - 1, 3, false, false, 0);
	starts = 0;
	assert(image_svg_start(NULL, literal, (monitor_scale_factor){4},
	    0, 0, NULL) == NULL && errno == EINVAL && starts == 0);
	mode = 1;
	check(xml, sizeof(xml) - 1, 4, false, 1);
	assert(errno == EPROTO);
	check_async(xml, sizeof(xml) - 1, 4, true, false, 1);
	mode = 2;
	check(xml, sizeof(xml) - 1, 4, false, 1);
	assert(errno == ENOMEM);
	check_async(xml, sizeof(xml) - 1, 4, false, false, 1);
	assert(errno == ENOMEM);
	mode = 0;
	isTerminated = 1;
	check(xml, sizeof(xml) - 1, 4, true, 1);
	return 0;
}
