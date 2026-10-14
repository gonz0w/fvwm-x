/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/image_process.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_MEMORY (128ULL * 1048576ULL)
#if defined(__OpenBSD__)
#define TEST_RESOURCE RLIMIT_DATA
#else
#define TEST_RESOURCE RLIMIT_AS
#endif

static bool fail_limit, unbounded_limit;
static unsigned limit_reads;
int __real_getrlimit(int resource, struct rlimit *limit);

int
__wrap_getrlimit(int resource, struct rlimit *limit)
{
	if (fail_limit)
	{
		errno = EPERM;
		return -1;
	}
	int result = __real_getrlimit(resource, limit);
	if (result == 0 && resource == TEST_RESOURCE && unbounded_limit &&
	    ++limit_reads > 1)
	{
		limit->rlim_cur = RLIM_INFINITY;
	}
	return result;
}

#if defined(__linux__)
/* Large-file configuration redirects getrlimit to this ELF symbol. */
int __real_getrlimit64(int resource, struct rlimit *limit);

int
__wrap_getrlimit64(int resource, struct rlimit *limit)
{
	if (fail_limit)
	{
		errno = EPERM;
		return -1;
	}
	int result = __real_getrlimit64(resource, limit);
	if (result == 0 && resource == TEST_RESOURCE && unbounded_limit &&
	    ++limit_reads > 1)
	{
		limit->rlim_cur = RLIM_INFINITY;
	}
	return result;
}
#endif

typedef struct
{
	uint64_t words[4];
	size_t received;
	bool reject;
} result_sink;

static bool
collect(const unsigned char *bytes, size_t length, void *context)
{
	result_sink *result = context;

	if (result->reject)
	{
		errno = EPROTO;
		return false;
	}
	assert(length <= sizeof(result->words) - result->received);
	memcpy((unsigned char *)result->words + result->received, bytes, length);
	result->received += length;
	return true;
}

static int
descriptors(void)
{
	int count = 0;

	for (int fd = 0; fd < 256; fd++)
	{
		if (fcntl(fd, F_GETFD) >= 0 || errno != EBADF)
		{
			count++;
		}
	}
	return count;
}

static bool
run(char *program, uint64_t memory, result_sink *result)
{
	image_input empty = {0};
	char *args[] = {program, "report", NULL};
	int before = descriptors();
	bool success = image_process_run(&empty, program, args,
	    sizeof(result->words), 3000, memory, collect, result, NULL, NULL);

	assert(before == descriptors());
	assert(waitpid(-1, NULL, WNOHANG) == -1 && errno == ECHILD);
	return success;
}

static int
report(void)
{
	struct rlimit memory, core;
	uint64_t words[4];
	unsigned char byte;

	assert(read(STDIN_FILENO, &byte, 1) == 0);
	assert(getrlimit(TEST_RESOURCE, &memory) == 0);
	assert(getrlimit(RLIMIT_CORE, &core) == 0);
	assert(memory.rlim_cur <= TEST_MEMORY && core.rlim_cur == 0);
	void *large = mmap(NULL, TEST_MEMORY * 2, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANON, -1, 0);

	assert(large == MAP_FAILED && errno == ENOMEM);
	void *small = mmap(NULL, 1048576, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANON, -1, 0);

	assert(small != MAP_FAILED);
	memset(small, 0x5a, 1048576);
	assert(munmap(small, 1048576) == 0);
	words[0] = (uint64_t)memory.rlim_cur;
	words[1] = (uint64_t)core.rlim_cur;
	words[2] = 0x4656574d;
	words[3] = alarm(0);
	assert(words[3] > 0 && words[3] <= 3);
	assert(write(STDOUT_FILENO, words, sizeof(words)) == sizeof(words));
	return 0;
}

int
main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "report") == 0)
	{
		return report();
	}
	char *program = realpath(argv[0], NULL);
	result_sink result = {0};

	assert(program != NULL && run(program, TEST_MEMORY, &result));
	assert(result.received == sizeof(result.words));
	assert(result.words[0] <= TEST_MEMORY && result.words[1] == 0);
	assert(result.words[2] == 0x4656574d);
	assert(result.words[3] > 0 && result.words[3] <= 3);
	struct rlimit original, lower;

	assert(getrlimit(TEST_RESOURCE, &original) == 0);
	lower = original;
	if (lower.rlim_cur == RLIM_INFINITY || lower.rlim_cur > TEST_MEMORY / 2)
	{
		lower.rlim_cur = TEST_MEMORY / 2;
	}
	assert(setrlimit(TEST_RESOURCE, &lower) == 0);
	memset(&result, 0, sizeof(result));
	assert(run(program, TEST_MEMORY, &result));
	assert(result.words[0] <= (uint64_t)lower.rlim_cur);
	assert(setrlimit(TEST_RESOURCE, &original) == 0);
	memset(&result, 0, sizeof(result));
	fail_limit = true;
	assert(!run(program, TEST_MEMORY, &result));
	assert(result.received == 0);
	fail_limit = false;
	unbounded_limit = true;
	limit_reads = 0;
	assert(!run(program, TEST_MEMORY, &result));
	assert(result.received == 0);
	unbounded_limit = false;
	result.reject = true;
	assert(!run(program, TEST_MEMORY, &result));
	result.reject = false;
	assert(run(program, TEST_MEMORY, &result));
	free(program);
	return 0;
}
