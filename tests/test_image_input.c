/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/image_input.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#if defined(__GLIBC__) && defined(__USE_FILE_OFFSET64)
#define __wrap_open __wrap_open64
#define __real_open __real_open64
#define __wrap_fcntl __wrap_fcntl64
#define __real_fcntl __real_fcntl64
#define __wrap_fstat __wrap_fstat64
#define __real_fstat __real_fstat64
#endif

enum injection
{
	NORMAL, OPEN_ERROR, FLAG_GET_ERROR, FLAG_SET_ERROR, STAT_ERROR,
	NEGATIVE_SIZE, BIG_SIZE, READ_ERROR, READ_INTERRUPTED, SHORT_READ,
	CLOSE_ERROR, ALLOC_ERROR, GROW_ALLOC_ERROR, GROW_TOO_FAR,
	GROW_WITHIN_LIMIT, SHRINK, REPLACE_PATH
};
static enum injection fault;
static int active, descriptor, open_calls, close_calls, read_calls;
static int allocation_calls;
static void *allocation;
static const char *source_path;

int __real_open(const char *path, int flags, ...);
int __real_fcntl(int fd, int command, ...);
int __real_fstat(int fd, struct stat *status);
ssize_t __real_read(int fd, void *bytes, size_t count);
int __real_close(int fd);
void *__real_realloc(void *bytes, size_t count);
void __real_free(void *bytes);

int
__wrap_open(const char *path, int flags, ...)
{
	/* Fixtures use mkstemp or __real_open; this is the production open. */
	assert(active && !(flags & O_CREAT));
	assert((flags & (O_NONBLOCK | O_NOCTTY | O_CLOEXEC)) ==
	    (O_NONBLOCK | O_NOCTTY | O_CLOEXEC));
	open_calls++;
	if (fault == OPEN_ERROR)
	{
		errno = EACCES;
		return -1;
	}
	descriptor = __real_open(path, flags);
	return descriptor;
}

int
__wrap_fcntl(int fd, int command, ...)
{
	int argument = 0;
	assert(active && fd == descriptor);
	assert(command == F_GETFD || command == F_SETFD);
	if (command == F_SETFD)
	{
		va_list args;
		va_start(args, command);
		argument = va_arg(args, int);
		va_end(args);
	}
	if ((fault == FLAG_GET_ERROR && command == F_GETFD) ||
	    (fault == FLAG_SET_ERROR && command == F_SETFD))
	{
		errno = EIO;
		return -1;
	}
	return __real_fcntl(fd, command, argument);
}

int
__wrap_fstat(int fd, struct stat *status)
{
	assert(active && fd == descriptor);
	assert(__real_fcntl(fd, F_GETFD) & FD_CLOEXEC);
	assert(__real_fcntl(fd, F_GETFL) & O_NONBLOCK);
	if (fault == STAT_ERROR)
	{
		errno = EIO;
		return -1;
	}
	int result = __real_fstat(fd, status);
	if (result == 0 && fault == NEGATIVE_SIZE)
	{
		status->st_size = -1;
	}
	if (result == 0 && fault == BIG_SIZE)
	{
		status->st_size = 100000;
	}
	return result;
}

ssize_t
__wrap_read(int fd, void *bytes, size_t count)
{
	assert(active && fd == descriptor && count <= 65536);
	read_calls++;
	if (fault == READ_INTERRUPTED && read_calls == 1)
	{
		errno = EINTR;
		return -1;
	}
	if (fault == READ_ERROR && read_calls == 2)
	{
		errno = EIO;
		return -1;
	}
	if (read_calls == 1)
	{
		if (fault == GROW_TOO_FAR || fault == GROW_WITHIN_LIMIT)
		{
			int writer = __real_open(source_path,
			    O_WRONLY | O_APPEND);
			assert(writer >= 0);
			assert(write(writer, "56789", 5) == 5);
			assert(__real_close(writer) == 0);
		}
		if (fault == SHRINK)
		{
			assert(truncate(source_path, 2) == 0);
		}
		if (fault == REPLACE_PATH)
		{
			assert(unlink(source_path) == 0);
			int replacement = __real_open(source_path,
			    O_WRONLY | O_CREAT | O_EXCL, 0600);
			assert(replacement >= 0);
			assert(write(replacement, "other", 5) == 5);
			assert(__real_close(replacement) == 0);
		}
	}
	if ((fault == SHORT_READ || fault == READ_ERROR) && count > 2)
	{
		count = 2;
	}
	return __real_read(fd, bytes, count);
}

#if defined(__GLIBC__)
/* Clang's fortified no-mode open uses these entry points. Preserve the
 * same injected descriptor ownership and failures as ordinary open. */
int
__wrap___open_2(const char *path, int flags)
{
	return __wrap_open(path, flags);
}

int
__wrap___open64_2(const char *path, int flags)
{
	return __wrap_open(path, flags);
}

/* Fortified bounded stack reads use this entry point instead of read. Keep
 * the same fault injection on both production acquisition paths. */
ssize_t
__wrap___read_chk(int fd, void *bytes, size_t count, size_t capacity)
{
	assert(count <= capacity);
	return __wrap_read(fd, bytes, count);
}
#endif

int
__wrap_close(int fd)
{
	assert(active && fd == descriptor);
	close_calls++;
	int result = __real_close(fd);
	if (fault == CLOSE_ERROR)
	{
		errno = EIO;
		return -1;
	}
	/* Setup/read failure must survive teardown's changed errno. */
	errno = EBADF;
	return result;
}

void *
__wrap_realloc(void *bytes, size_t count)
{
	assert(active && bytes == allocation);
	allocation_calls++;
	if (fault == ALLOC_ERROR ||
	    (fault == GROW_ALLOC_ERROR && allocation_calls == 2))
	{
		errno = ENOMEM;
		return NULL;
	}
	void *result = __real_realloc(bytes, count);
	if (result != NULL)
	{
		allocation = result;
	}
	return result;
}

void
__wrap_free(void *bytes)
{
	if (bytes != NULL && bytes == allocation)
	{
		allocation = NULL;
	}
	__real_free(bytes);
}

static void
write_fixture(const char *path, size_t count)
{
	int fd = __real_open(path, O_WRONLY | O_TRUNC);
	assert(fd >= 0);
	unsigned char bytes[1024];
	for (size_t i = 0; i < sizeof(bytes); i++)
	{
		bytes[i] = (unsigned char)('1' + i % 4);
	}
	while (count > 0)
	{
		size_t chunk = count < sizeof(bytes) ? count : sizeof(bytes);
		assert(write(fd, bytes, chunk) == (ssize_t)chunk);
		count -= chunk;
	}
	assert(__real_close(fd) == 0);
}

static void
check_load(const char *path, size_t limit, enum injection mode,
    int expected_errno, const char *expected, size_t expected_length)
{
	image_input input = {0};
	struct stat before = {0}, metadata;
	int had_file = stat(path, &before) == 0;

	memset(&metadata, 0xff, sizeof(metadata));
	assert(allocation == NULL);
	active = 1;
	fault = mode;
	descriptor = -1;
	open_calls = close_calls = read_calls = allocation_calls = 0;
	source_path = path;
	bool result = image_input_load(path, limit, &input, &metadata);
	int error = errno;
	assert(result == (expected_errno == 0));
	if (result)
	{
		assert(input.bytes != NULL &&
		    input.length == expected_length);
		assert(input.bytes[input.length] == 0);
		assert(had_file && metadata.st_dev == before.st_dev &&
		    metadata.st_ino == before.st_ino &&
		    metadata.st_size == before.st_size);
		if (expected != NULL)
		{
			assert(memcmp(input.bytes, expected,
			    expected_length) == 0);
		}
	}
	else
	{
		if (error != expected_errno)
		{
			fprintf(stderr,
			    "%s: mode %d, expected errno %d, got %d\n",
			    path, mode, expected_errno, error);
		}
		assert(error == expected_errno);
		assert(input.bytes == NULL && input.length == 0);
		assert(metadata.st_mode == 0 && metadata.st_size == 0 &&
		    metadata.st_ino == 0);
	}
	image_input_destroy(&input);
	image_input_destroy(&input);
	assert(allocation == NULL);
	assert(close_calls == (descriptor >= 0));
	if (descriptor >= 0)
	{
		assert(__real_fcntl(descriptor, F_GETFD) == -1 &&
		    errno == EBADF);
	}
	active = 0;
}

static void
test_byte_boundaries(const char *path)
{
	for (size_t count = 0; count <= 9; count++)
	{
		write_fixture(path, count);
		check_load(path, 8, NORMAL, count > 8 ? EFBIG : 0, NULL,
		    count);
	}
	int fd = __real_open(path, O_WRONLY | O_TRUNC);
	assert(fd >= 0);
	assert(ftruncate(fd, (off_t)IMAGE_INPUT_DEFAULT_LIMIT + 1) == 0);
	assert(__real_close(fd) == 0);
	check_load(path, IMAGE_INPUT_DEFAULT_LIMIT, NORMAL, EFBIG, NULL, 0);
	assert(allocation_calls == 0 && read_calls == 0);
	write_fixture(path, 70000);
	check_load(path, 70000, NORMAL, 0, NULL, 70000);
	check_load(path, 70000, GROW_ALLOC_ERROR, ENOMEM, NULL, 0);
}

static void
test_failures_and_short_reads(const char *path)
{
	const enum injection faults[] = {OPEN_ERROR, FLAG_GET_ERROR,
		FLAG_SET_ERROR, STAT_ERROR, NEGATIVE_SIZE, BIG_SIZE,
		READ_ERROR, CLOSE_ERROR, ALLOC_ERROR};
	write_fixture(path, 4);
	for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); i++)
	{
		int error = faults[i] == OPEN_ERROR ? EACCES :
		    faults[i] == ALLOC_ERROR ? ENOMEM :
		    (faults[i] == NEGATIVE_SIZE || faults[i] == BIG_SIZE) ?
			EFBIG : EIO;
		check_load(path, 8, faults[i], error, NULL, 0);
	}
	check_load(path, 8, READ_INTERRUPTED, 0, "1234", 4);
	check_load(path, 8, SHORT_READ, 0, "1234", 4);
	check_load(path, 8, GROW_TOO_FAR, EFBIG, NULL, 0);
	write_fixture(path, 4);
	check_load(path, 9, GROW_WITHIN_LIMIT, 0, "123456789", 9);
	write_fixture(path, 4);
	check_load(path, 8, SHRINK, 0, "12", 2);
	write_fixture(path, 4);
	check_load(path, 8, REPLACE_PATH, 0, "1234", 4);
	/* The snapshot retained the old inode after path replacement. */
	check_load(path, 8, NORMAL, 0, "other", 5);
}

static void
test_types_and_links(const char *directory, const char *path)
{
	char link_path[256], fifo_path[256], socket_path[256];
	assert(snprintf(link_path, sizeof(link_path),
	    "%s/link", directory) > 0);
	assert(snprintf(fifo_path, sizeof(fifo_path),
	    "%s/fifo", directory) > 0);
	assert(snprintf(socket_path, sizeof(socket_path),
	    "%s/socket", directory) > 0);
	assert(symlink(path, link_path) == 0);
	check_load(link_path, 8, NORMAL, 0, "other", 5);
	assert(unlink(link_path) == 0);
	assert(link(path, link_path) == 0);
	check_load(link_path, 8, NORMAL, 0, "other", 5);
	assert(unlink(link_path) == 0);
	assert(mkfifo(fifo_path, 0600) == 0);
	check_load(fifo_path, 8, NORMAL, EINVAL, NULL, 0);
	assert(symlink(fifo_path, link_path) == 0);
	check_load(link_path, 8, NORMAL, EINVAL, NULL, 0);
	assert(unlink(link_path) == 0 && unlink(fifo_path) == 0);
	check_load(directory, 8, NORMAL, EINVAL, NULL, 0);
	check_load("/dev/null", 8, NORMAL, EINVAL, NULL, 0);
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	struct sockaddr_un address = {0};
	assert(fd >= 0);
	address.sun_family = AF_UNIX;
	assert(strlen(socket_path) < sizeof(address.sun_path));
	memcpy(address.sun_path, socket_path, strlen(socket_path) + 1);
	assert(bind(fd, (struct sockaddr *)&address, sizeof(address)) == 0);
	#if defined(__linux__)
	check_load(socket_path, 8, NORMAL, ENXIO, NULL, 0);
#else
	check_load(socket_path, 8, NORMAL, EOPNOTSUPP, NULL, 0);
#endif
	assert(__real_close(fd) == 0 && unlink(socket_path) == 0);
	check_load(link_path, 8, NORMAL, ENOENT, NULL, 0);
}

static void
test_invalid_arguments(void)
{
	image_input input = {(unsigned char *)(uintptr_t)1, 123};
	assert(!image_input_load(NULL, 8, &input, NULL) && errno == EINVAL);
	assert(input.bytes == NULL && input.length == 0);
	assert(!image_input_load("unused", 0, &input, NULL) && errno == EINVAL);
	assert(!image_input_load("unused", SIZE_MAX, &input, NULL) &&
	    errno == EINVAL);
	assert(!image_input_load("unused", 8, NULL, NULL) && errno == EINVAL);
	image_input_destroy(NULL);
}

static void
test_prefix(const char *path)
{
	write_fixture(path, 100000);
	const enum injection modes[] = {NORMAL, READ_INTERRUPTED, SHORT_READ,
	    OPEN_ERROR, FLAG_GET_ERROR, FLAG_SET_ERROR, STAT_ERROR,
	    NEGATIVE_SIZE, BIG_SIZE, READ_ERROR, CLOSE_ERROR};

	for (unsigned i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
	{
		image_input_prefix prefix;
		struct stat metadata;
		struct stat before;

		assert(stat(path, &before) == 0);
		memset(&prefix, 0xff, sizeof(prefix));
		memset(&metadata, 0xff, sizeof(metadata));
		active = 1;
		fault = modes[i];
		descriptor = -1;
		open_calls = close_calls = read_calls = allocation_calls = 0;
		source_path = path;
		size_t limit = fault == BIG_SIZE ? 99999 : 100000;
		bool result = image_input_probe(path, limit,
		    &prefix, &metadata);

		assert(allocation_calls == 0 && allocation == NULL);
		assert(open_calls == 1);
		assert(close_calls == (fault == OPEN_ERROR ? 0 : 1));
		if (i < 3)
		{
			assert(result && prefix.length == sizeof(prefix.bytes));
			assert(memcmp(prefix.bytes,
			    "1234123412341234", 16) == 0);
			assert(metadata.st_ino == before.st_ino);
			assert(metadata.st_size == before.st_size);
		}
		else
		{
			if (result || prefix.length != 0)
			{
				fprintf(stderr,
				    "prefix fault=%u result=%d length=%zu\n",
				    (unsigned)fault, result, prefix.length);
			}
			assert(!result && prefix.length == 0);
			assert(metadata.st_ino == 0 && metadata.st_size == 0);
		}
		active = 0;
	}
	image_input_prefix prefix;

	assert(!image_input_probe(NULL, 1, &prefix, NULL) && errno == EINVAL);
	assert(!image_input_probe(path, 0, &prefix, NULL) && errno == EINVAL);
	assert(!image_input_probe(path, SIZE_MAX, &prefix, NULL) &&
	    errno == EINVAL);
	assert(!image_input_probe(path, 1, NULL, NULL) && errno == EINVAL);
	write_fixture(path, 0);
	active = 1;
	fault = NORMAL;
	assert(image_input_probe(path, 1, &prefix, NULL) && prefix.length == 0);
	active = 0;
}

int
main(void)
{
	char directory[] = "/tmp/fvwm-image-input-XXXXXX";
	char path[256];
	/* A missed nonblocking open must fail instead of stalling. */
	alarm(10);
	assert(mkdtemp(directory) != NULL);
	assert(snprintf(path, sizeof(path), "%s/image", directory) > 0);
	int fd = __real_open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
	assert(fd >= 0 && __real_close(fd) == 0);
	test_prefix(path);
	test_invalid_arguments();
	test_byte_boundaries(path);
	fd = __real_open(path, O_WRONLY | O_TRUNC);
	assert(fd >= 0 && write(fd, "a\0b", 3) == 3);
	assert(__real_close(fd) == 0);
	check_load(path, 3, NORMAL, 0, "a\0b", 3);
	test_failures_and_short_reads(path);
	test_types_and_links(directory, path);
	assert(unlink(path) == 0 && rmdir(directory) == 0);
	puts("bounded snapshots preserve file identity and clean up failures");
	return 0;
}
