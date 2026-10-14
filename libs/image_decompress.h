#ifndef FVWMLIB_IMAGE_DECOMPRESS_H
#define FVWMLIB_IMAGE_DECOMPRESS_H

#include "image_input.h"

#define IMAGE_DECOMPRESS_MAX_TIMEOUT_MS 60000

typedef struct image_process image_process;
typedef struct
{
	image_input output;
	size_t capacity;
	size_t limit;
} image_decompress_result;

/* Start uses the same bounded collector as the synchronous adapter. Result
 * starts empty and remains owned until the returned process is reaped; output
 * is publishable only after IMAGE_PROCESS_SUCCEEDED. No wait occurs here. */
image_process *image_decompress_start(const image_input *source,
	const char *program, char *const argv[], size_t limit, int timeout_ms,
	image_decompress_result *result);
void image_decompress_result_destroy(image_decompress_result *result);

typedef bool (*image_decompress_cancel_fn)(void *context);

/* Literal trusted executable/argv, checked source snapshot, finite expanded
 * bytes and overall transport/completion deadline. Failure clears output and
 * sets errno. Own only this child; restore caller signal state on every path.
 * This replaces an existing synchronous decompressor wait, not a sandbox or
 * an asynchronous image service. */
bool image_decompress(const image_input *source, const char *program,
	char *const argv[], size_t limit, int timeout_ms,
	image_decompress_cancel_fn cancel, void *context, image_input *output);

#endif
