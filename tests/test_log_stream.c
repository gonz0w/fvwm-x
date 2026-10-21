/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "libs/log.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

static int fault, adoptions;
FILE *__real_fdopen(int, const char *);
int __real_fflush(FILE *);

FILE *
__wrap_fdopen(int fd, const char *mode)
{
	adoptions++;
	if (fault == 1)
	{
		errno = ENOMEM;
		return NULL;
	}
	return __real_fdopen(fd, mode);
}

int
__wrap_fflush(FILE *file)
{
	if (fault == 2)
	{
		errno = ENOSPC;
		return EOF;
	}
	return __real_fflush(file);
}

static void test_inherited_adoption(void)
{
	for (int mode = 0; mode < 3; mode++)
	{
		pid_t child = fork();
		int status;

		assert(child >= 0);
		if (child == 0)
		{
			char path[] = "/tmp/fvwm-inherited-log-XXXXXX";
			int inherited = mkstemp(path);
			struct stat file;

			assert(inherited >= 0 && unlink(path) == 0);
			assert(!log_adopt_inherited_fd(-2));
			if (mode == 0)
			{
				assert(log_adopt_inherited_fd(-1));
			}
			else if (mode == 1)
			{
				assert(log_adopt_inherited_fd(inherited));
				fault = 2;
				fvwm_debug(NULL, "sink failure");
				fault = 0;
				assert(log_get_fd() == -1);
				/* Reuse the log number for other storage. */
				char other[] = "/tmp/fvwm-reused-log-XXXXXX";
				int reused = mkstemp(other);

				assert(reused >= 0 && unlink(other) == 0);
				assert(dup2(reused, inherited) == inherited);
				if (reused != inherited)
					close(reused);
			}
			else
			{
				fault = 1;
				assert(!log_adopt_inherited_fd(inherited));
				fault = 0;
			}
			for (int repeat = 0; repeat < 100; repeat++)
			{
				assert(log_adopt_inherited_fd(inherited));
				fvwm_debug(NULL,
					"must not adopt another resource");
			}
			assert(log_get_fd() == -1);
			assert(fstat(inherited, &file) == 0 &&
			    file.st_size == 0);
			log_close();
			assert(fcntl(inherited, F_GETFD) >= 0);
			close(inherited);
			_exit(0);
		}
		assert(waitpid(child, &status, 0) == child);
		assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	}
}

int
main(int argc, char **argv)
{
	if (argc == 2)
	{
		assert(fcntl(atoi(argv[1]), F_GETFD) >= 0);
		return 0;
	}
	test_inherited_adoption();
	char directory[] = "/tmp/fvwm-log-stream-XXXXXX";
	char first[256], second[256], absent[256], content[1024];

	assert(mkdtemp(directory) != NULL);
	snprintf(first, sizeof(first), "%s/first", directory);
	snprintf(second, sizeof(second), "%s/second", directory);
	snprintf(absent, sizeof(absent), "%s/absent/log", directory);
	int old = open(first, O_CREAT | O_WRONLY, 0600);
	assert(old >= 0 && log_set_fd(old));
	for (int i = 0; i < 100; i++)
	{
		assert(log_set_fd(old));
	}
	assert(adoptions == 1 && log_get_fd() == old);
	assert((fcntl(old, F_GETFD) & FD_CLOEXEC) == 0);
	char number[32];
	snprintf(number, sizeof(number), "%d", old);
	pid_t child = fork();
	assert(child >= 0);
	if (child == 0)
	{
		execl(argv[0], argv[0], number, (char *)NULL);
		_exit(127);
	}
	int child_status;
	assert(waitpid(child, &child_status, 0) == child);
	assert(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);
	fvwm_debug(NULL, "%s", "");
	fvwm_debug("owner", "retained");
	int candidate = open(second, O_CREAT | O_WRONLY, 0600);
	assert(candidate >= 0);
	fault = 1;
	assert(!log_set_fd(candidate) && errno == ENOMEM);
	assert(log_get_fd() == old && fcntl(candidate, F_GETFD) >= 0);
	fault = 0;
	close(candidate);
	lib_log_level = 1;
	set_log_file(absent);
	log_open(directory);
	assert(log_get_fd() == old);
	set_log_file(second);
	log_open(directory);
	assert(log_get_fd() >= 0 && log_get_fd() != old);
	assert(fcntl(old, F_GETFD) < 0 && errno == EBADF);
	int active = log_get_fd();
	fault = 2;
	fvwm_debug("owner", "full sink must not terminate the process");
	fault = 0;
	assert(log_get_fd() == -1);
	assert(fcntl(active, F_GETFD) < 0 && errno == EBADF);
	log_open(directory);
	assert(log_get_fd() >= 0);
	fvwm_debug(NULL, "recovered\n");
	active = log_get_fd();
	set_log_file("-");
	log_open(directory);
	assert(log_get_fd() == STDERR_FILENO);
	assert(fcntl(active, F_GETFD) < 0 && errno == EBADF);
	log_close();
	assert(fcntl(STDERR_FILENO, F_GETFD) >= 0);
	assert(log_set_fd(STDERR_FILENO));
	log_close();
	assert(fcntl(STDERR_FILENO, F_GETFD) >= 0);
	assert(log_set_fd(-1));
	assert(!log_set_fd(-2) && errno == EBADF);
	set_log_file(NULL);

	FILE *file = fopen(first, "r");
	assert(file != NULL);
	size_t count = fread(content, 1, sizeof(content) - 1, file);
	content[count] = '\0';
	assert(strstr(content, "owner: retained\n") != NULL);
	assert(strchr(content, '\n') < strstr(content, "owner: retained"));
	fclose(file);
	assert(unlink(first) == 0 && unlink(second) == 0);
	assert(rmdir(directory) == 0);
	puts("log stream: repeated adoption, rollback, empty format, "
	    "sink failure, recovery");
	return 0;
}
