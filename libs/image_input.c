/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "image_input.h"
#include "fd_safety.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define IMAGE_INPUT_READ_CHUNK 65536U

void
image_input_destroy(image_input *input)
{
	if (input != NULL)
	{
		free(input->bytes);
		input->bytes = NULL;
		input->length = 0;
	}
}

/* Share the open policy between prefix classification and full snapshots. */
static int
image_input_open(const char *path, size_t limit, struct stat *status)
{
	int descriptor;
	int saved_errno;

	/* Nonblocking opening is essential: fstat cannot reject an empty FIFO
	 * until open has returned. Do not impose lock-file ownership rules on
	 * shared theme files or symlinks to regular images. */
	descriptor = open(path, O_RDONLY | O_NONBLOCK | O_NOCTTY | O_CLOEXEC);
	if (descriptor < 0)
	{
		return -1;
	}
	if (!fvwm_fd_set_cloexec(descriptor, true) ||
	    fstat(descriptor, status) != 0)
	{
		goto refuse;
	}
	if (!S_ISREG(status->st_mode))
	{
		errno = EINVAL;
		goto refuse;
	}
	if (status->st_size < 0 || (uintmax_t)status->st_size > limit)
	{
		errno = EFBIG;
		goto refuse;
	}
	return descriptor;

refuse:
	saved_errno = errno;
	close(descriptor);
	errno = saved_errno;
	return -1;
}

bool
image_input_probe(const char *path, size_t limit, image_input_prefix *prefix,
	struct stat *metadata)
{
	struct stat status = {0};
	image_input_prefix snapshot = {0};
	int descriptor;
	int error;

	if (metadata != NULL)
	{
		memset(metadata, 0, sizeof(*metadata));
	}
	if (prefix == NULL)
	{
		errno = EINVAL;
		return false;
	}
	memset(prefix, 0, sizeof(*prefix));
	if (path == NULL || limit == 0 || limit == SIZE_MAX)
	{
		errno = EINVAL;
		return false;
	}
	descriptor = image_input_open(path, limit, &status);
	if (descriptor < 0)
	{
		return false;
	}
	while (snapshot.length < sizeof(snapshot.bytes))
	{
		ssize_t count = read(descriptor,
		    snapshot.bytes + snapshot.length,
		    sizeof(snapshot.bytes) - snapshot.length);

		if (count < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			error = errno;
			close(descriptor);
			errno = error;
			return false;
		}
		if (count == 0)
		{
			break;
		}
		snapshot.length += (size_t)count;
	}
	if (close(descriptor) != 0)
	{
		return false;
	}
	*prefix = snapshot;
	if (metadata != NULL)
	{
		*metadata = status;
	}
	return true;
}

bool
image_input_load(const char *path, size_t limit, image_input *input,
	struct stat *metadata)
{
	struct stat status = {0};
	image_input snapshot = {0};
	size_t capacity = 0;
	int descriptor;
	int saved_errno;

	if (metadata != NULL)
	{
		memset(metadata, 0, sizeof(*metadata));
	}
	if (input == NULL)
	{
		errno = EINVAL;
		return false;
	}
	input->bytes = NULL;
	input->length = 0;
	if (path == NULL || limit == 0 || limit == SIZE_MAX)
	{
		errno = EINVAL;
		return false;
	}
	descriptor = image_input_open(path, limit, &status);
	if (descriptor < 0)
	{
		return false;
	}
	for (;;)
	{
		ssize_t count;
		unsigned char extra;

		if (snapshot.length == limit)
		{
			/* Detect growth after fstat without exceeding the cap.
			 * Distinguish exact-limit EOF from overflow. */
			count = read(descriptor, &extra, 1);
		}
		else
		{
			if (snapshot.length == capacity)
			{
				size_t next = capacity == 0 ?
					IMAGE_INPUT_READ_CHUNK : capacity;
				unsigned char *bytes;

				if (capacity != 0)
				{
					next = capacity > limit / 2 ?
						limit : capacity * 2;
				}
				if (next > limit)
				{
					next = limit;
				}
				bytes = realloc(snapshot.bytes, next + 1);
				if (bytes == NULL)
				{
					goto fail;
				}
				snapshot.bytes = bytes;
				capacity = next;
			}
			size_t available = capacity - snapshot.length;

			if (available > IMAGE_INPUT_READ_CHUNK)
			{
				available = IMAGE_INPUT_READ_CHUNK;
			}
			count = read(descriptor,
			    snapshot.bytes + snapshot.length, available);
		}
		if (count < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			goto fail;
		}
		if (count == 0)
		{
			break;
		}
		if (snapshot.length == limit)
		{
			errno = EFBIG;
			goto fail;
		}
		snapshot.length += (size_t)count;
	}
	/* Do not publish data if closing reports an I/O failure. Never retry
	 * close: whether EINTR closed the descriptor differs across systems. */
	if (close(descriptor) != 0)
	{
		saved_errno = errno;
		image_input_destroy(&snapshot);
		errno = saved_errno;
		return false;
	}
	snapshot.bytes[snapshot.length] = '\0';
	*input = snapshot;
	if (metadata != NULL)
	{
		*metadata = status;
	}
	return true;

fail:
	saved_errno = errno;
	close(descriptor);
	image_input_destroy(&snapshot);
	errno = saved_errno;
	return false;
}
