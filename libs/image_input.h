#ifndef FVWMLIB_IMAGE_INPUT_H
#define FVWMLIB_IMAGE_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>

#define IMAGE_INPUT_DEFAULT_LIMIT (128U * 1024U * 1024U)

typedef struct
{
	unsigned char *bytes;
	size_t length;
} image_input;

typedef struct
{
	unsigned char bytes[16];
	size_t length;
} image_input_prefix;

/* Same checked open and source cap, at most 16 bytes without allocation.
 * Metadata records the opened identity; both outputs are empty on refusal.
 * Classification never retains full source buffers for queued requests. */
bool image_input_probe(const char *path, size_t limit,
	image_input_prefix *prefix, struct stat *metadata);

/* Read one opened regular-file identity, following symlinks. limit excludes
 * the trailing NUL. Failure leaves input empty and sets errno. The caller
 * supplies an empty output and owns the immutable snapshot on success.
 * This bounds source bytes, not storage latency, decoder work or resources
 * subsequently loaded by a decoder. */
/* Optional metadata describes the initially opened descriptor and is cleared
 * on failure. It avoids stamping decoded bytes from a replaced pathname. */
bool image_input_load(const char *path, size_t limit, image_input *input,
	struct stat *metadata);
void image_input_destroy(image_input *input);

#endif
