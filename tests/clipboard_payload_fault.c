/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <errno.h>
#include <sys/mman.h>
#include <time.h>

/* Separate test executable only. There is no runtime fault switch in the
 * installed child. The delay build slows protection; the other build refuses
 * release persistently so its terminal cleanup cannot
 * be mislabeled a pass, while preserving real mapping/lock/protection work. */
void *content_payload_test_map(void *address, size_t size, int protection,
	int flags, int descriptor, off_t offset)
{
	return mmap(address, size, protection, flags, descriptor, offset);
}
int content_payload_test_protect(void *address, size_t size, int protection)
{
	return mprotect(address, size, protection);
}
int content_payload_test_advise(void *address, size_t size, int advice)
{
	return madvise(address, size, advice);
}
int content_payload_test_lock(const void *address, size_t size)
{
#ifdef CLIPBOARD_TEST_DELAY
	struct timespec delay = {.tv_nsec = 450000000};

	while (nanosleep(&delay, &delay) < 0 && errno == EINTR)
		;
#endif
	return mlock(address, size);
}
int content_payload_test_unlock(const void *address, size_t size)
{
	return munlock(address, size);
}
int content_payload_test_unmap(void *address, size_t size)
{
#ifdef CLIPBOARD_TEST_DELAY
	return munmap(address, size);
#else
	(void)address;
	(void)size;
	errno = EIO;
	return -1;
#endif
}
