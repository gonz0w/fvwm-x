/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "image_process.h"
#include "fd_safety.h"
#include "fvwmsignal.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define IMAGE_PROCESS_CHUNK 65536U

static int64_t
image_process_now(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
	{
		return -1;
	}
	if (now.tv_sec < 0 || (uint64_t)now.tv_sec >
	    (INT64_MAX - IMAGE_DECOMPRESS_MAX_TIMEOUT_MS - 999) / 1000)
	{
		errno = EOVERFLOW;
		return -1;
	}
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static bool
image_process_close(int *fd)
{
	int owned = *fd;

	*fd = -1;
	return owned < 0 || close(owned) == 0;
}

static bool
image_process_pipe(int endpoints[2])
{
	if (!fvwm_pipe_cloexec(endpoints))
	{
		return false;
	}
	/* dup2 must not overwrite another endpoint when stdio was closed. */
	for (int i = 0; i < 2; i++)
	{
		if (endpoints[i] <= STDERR_FILENO)
		{
			int above = fcntl(endpoints[i], F_DUPFD_CLOEXEC, 3);

			if (above < 0)
			{
				return false;
			}
			int old = endpoints[i];

			endpoints[i] = above;
			if (close(old) < 0)
			{
				return false;
			}
		}
	}
	return true;
}

static bool
image_process_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL);

	return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

static bool
image_process_memory_limit(uint64_t bytes)
{
#if defined(__OpenBSD__)
	const int resource = RLIMIT_DATA;
#else
	const int resource = RLIMIT_AS;
#endif
	struct rlimit limit;
	rlim_t requested = (rlim_t)bytes;

	if ((uint64_t)requested != bytes || requested == RLIM_INFINITY ||
	    getrlimit(resource, &limit) < 0)
	{
		return false;
	}
	if (limit.rlim_cur == RLIM_INFINITY || requested < limit.rlim_cur)
	{
		limit.rlim_cur = requested;
	}
	if (limit.rlim_max != RLIM_INFINITY && limit.rlim_cur > limit.rlim_max)
	{
		limit.rlim_cur = limit.rlim_max;
	}
	/* Keep both soft and hard bounds finite in the worker. */
	if (limit.rlim_max == RLIM_INFINITY || limit.rlim_max > limit.rlim_cur)
	{
		limit.rlim_max = limit.rlim_cur;
	}
	struct rlimit applied;

	if (setrlimit(resource, &limit) < 0 ||
	    getrlimit(resource, &applied) < 0)
	{
		return false;
	}
	if (applied.rlim_cur == RLIM_INFINITY ||
	    applied.rlim_cur > limit.rlim_cur ||
	    applied.rlim_max == RLIM_INFINITY ||
	    applied.rlim_max > limit.rlim_max)
	{
		errno = EPERM;
		return false;
	}
	if (getrlimit(RLIMIT_CORE, &limit) < 0)
	{
		return false;
	}
	limit.rlim_cur = 0;
	limit.rlim_max = 0;
	if (setrlimit(RLIMIT_CORE, &limit) < 0 ||
	    getrlimit(RLIMIT_CORE, &applied) < 0 || applied.rlim_cur != 0 ||
	    applied.rlim_max != 0)
	{
		errno = EPERM;
		return false;
	}
	return true;
}

struct image_process
{
	const image_input *source;
	image_process_sink_fn sink;
	void *sink_context;
	pid_t child;
	int input;
	int output;
	int status;
	int error;
	int64_t deadline;
	size_t used;
	size_t received;
	size_t limit;
	bool watched;
	bool killed;
	bool exited;
};

static bool image_process_block(sigset_t *previous)
{
	sigset_t blocked;

	sigemptyset(&blocked);
	sigaddset(&blocked, SIGCHLD);
	return sigprocmask(SIG_BLOCK, &blocked, previous) == 0;
}

/* Caller blocks SIGCHLD across observing exit and signalling: a reaped PID
 * must never become a cancellation target after it can be reused. */
static void image_process_reap(image_process *process)
{
	int status;
	pid_t child = process->child;

	if (child <= 0)
	{
		return;
	}
	bool captured = process->watched && fvwm_child_status(child, &status);
	pid_t waited = captured ? child : waitpid(child, &status, WNOHANG);

	if (waited == child)
	{
		if (process->watched)
		{
			fvwm_child_unwatch(child);
			process->watched = false;
		}
		process->child = -1;
		process->exited = true;
		process->status = status;
	}
	else if (waited < 0 && errno != EINTR)
	{
		/* A wait error loses ownership. Never signal that numeric PID.
		 * Automatic/custom reapers are rejected at startup. */
		if (process->watched)
		{
			fvwm_child_unwatch(child);
			process->watched = false;
		}
		process->child = -1;
		process->error = process->error ? process->error : errno;
	}
}

static void image_process_fail(image_process *process, int error)
{
	if (process->error == 0)
	{
		process->error = error ? error : EIO;
	}
	image_process_close(&process->input);
	image_process_close(&process->output);
	image_process_reap(process);
	if (process->child > 0 && !process->killed)
	{
		/* Retain PID/group ownership through both signals. */
		kill(-process->child, SIGKILL);
		kill(process->child, SIGKILL);
		process->killed = true;
	}
}

image_process *image_process_start(const image_input *source,
	const char *program, char *const argv[], size_t limit, int timeout_ms,
	uint64_t memory_limit, image_process_sink_fn sink, void *sink_context)
{
	int input_pipe[2] = {-1, -1}, output_pipe[2] = {-1, -1};
	sigset_t previous;
	struct sigaction disposition, default_child = {0};
	image_process *process = NULL;
	pid_t child = -1;
	int error;
	int64_t now;
	long open_max;
	bool mask_saved = false;
	bool watch = false;

	if (source == NULL || (source->length && source->bytes == NULL) ||
	    program == NULL || program[0] != '/' || argv == NULL ||
	    argv[0] == NULL || sink == NULL || limit == 0 ||
	    limit == SIZE_MAX ||
	    timeout_ms < 1 || timeout_ms > IMAGE_DECOMPRESS_MAX_TIMEOUT_MS)
	{
		errno = EINVAL;
		return NULL;
	}
	now = image_process_now();
	if (now < 0)
	{
		return NULL;
	}
	open_max = sysconf(_SC_OPEN_MAX);
	if (open_max < 0 || open_max > INT_MAX)
	{
		errno = EOVERFLOW;
		return NULL;
	}
	process = calloc(1, sizeof(*process));
	if (process == NULL)
	{
		return NULL;
	}
	process->source = source;
	process->sink = sink;
	process->sink_context = sink_context;
	process->input = process->output = -1;
	process->child = -1;
	process->deadline = now + timeout_ms;
	process->limit = limit;
	if (!image_process_pipe(input_pipe) ||
	    !image_process_pipe(output_pipe) ||
	    !image_process_nonblock(input_pipe[1]) ||
	    !image_process_nonblock(output_pipe[0]) ||
	    !image_process_block(&previous))
	{
		goto failed;
	}
	mask_saved = true;
	if (sigaction(SIGCHLD, NULL, &disposition) < 0)
	{
		goto failed;
	}
	/* A synchronous adapter can keep any reaper blocked. An asynchronous
	 * caller needs default waitability or the atomic FVWM status watch. */
	if (!sigismember(&previous, SIGCHLD))
	{
		if (disposition.sa_handler == fvwmReapChildren &&
		    !(disposition.sa_flags & SA_NOCLDWAIT))
		{
			watch = true;
			if (!fvwm_child_watch_available())
			{
				errno = EBUSY;
				goto failed;
			}
		}
		else if (disposition.sa_handler != SIG_DFL ||
		    (disposition.sa_flags & SA_NOCLDWAIT))
		{
			errno = EINVAL;
			goto failed;
		}
	}
	default_child.sa_handler = SIG_DFL;
	sigemptyset(&default_child.sa_mask);
	child = fork();
	if (child < 0)
	{
		goto failed;
	}
	if (child == 0)
	{
		const int signals[] = {SIGCHLD, SIGTERM, SIGINT, SIGHUP,
			SIGQUIT, SIGPIPE};

		for (size_t i = 0;
		    i < sizeof(signals) / sizeof(signals[0]); i++)
		{
			if (sigaction(signals[i], &default_child, NULL) < 0)
			{
				_exit(127);
			}
		}
		if (setpgid(0, 0) < 0 ||
		    dup2(input_pipe[0], STDIN_FILENO) < 0 ||
		    dup2(output_pipe[1], STDOUT_FILENO) < 0)
		{
			_exit(127);
		}
		int quiet = open("/dev/null", O_WRONLY | O_NOCTTY);

		if (quiet < 0 || dup2(quiet, STDERR_FILENO) < 0)
		{
			_exit(127);
		}
#if HAVE_CLOSEFROM
		closefrom(3);
#else
		for (int fd = 3; fd < open_max; fd++)
		{
			close(fd);
		}
#endif
		sigset_t child_mask = previous;

		sigdelset(&child_mask, SIGCHLD);
		if (memory_limit != 0)
		{
			/* Bound the decoder if its owner dies during secondary
			 * I/O. alarm survives exec; the parent owns the precise
			 * deadline. */
			if (sigaction(SIGALRM, &default_child, NULL) < 0)
			{
				_exit(127);
			}
			sigdelset(&child_mask, SIGALRM);
			alarm((unsigned)(timeout_ms + 999) / 1000);
		}
		if (sigprocmask(SIG_SETMASK, &child_mask, NULL) < 0)
		{
			_exit(127);
		}
		if (memory_limit != 0 &&
		    !image_process_memory_limit(memory_limit))
		{
			_exit(127);
		}
		execv(program, argv);
		_exit(127);
	}

	process->child = child;
	process->input = input_pipe[1];
	process->output = output_pipe[0];
	input_pipe[1] = output_pipe[0] = -1;
	if (watch)
	{
		if (!fvwm_child_watch(child))
		{
			image_process_fail(process, errno);
		}
		else
		{
			process->watched = true;
		}
	}
	if (!image_process_close(&input_pipe[0]) ||
	    !image_process_close(&output_pipe[1]))
	{
		image_process_fail(process, errno);
	}
	if (setpgid(child, child) < 0 && errno != EACCES && errno != ESRCH)
	{
		image_process_fail(process, errno);
	}
	if (sigprocmask(SIG_SETMASK, &previous, NULL) < 0)
	{
		image_process_fail(process, errno);
	}
	image_process_close(&input_pipe[0]);
	image_process_close(&output_pipe[1]);
	return process;

failed:
	error = errno;
	image_process_close(&input_pipe[0]);
	image_process_close(&input_pipe[1]);
	image_process_close(&output_pipe[0]);
	image_process_close(&output_pipe[1]);
	if (mask_saved)
	{
		sigprocmask(SIG_SETMASK, &previous, NULL);
	}
	free(process);
	errno = error;
	return NULL;
}

void image_process_descriptors(const image_process *process,
	struct pollfd descriptors[2])
{
	descriptors[0] = (struct pollfd){process->output, POLLIN, 0};
	descriptors[1] = (struct pollfd){process->input, POLLOUT, 0};
}

int image_process_timeout_ms(const image_process *process)
{
	int64_t now = image_process_now();

	if (now < 0 || process->deadline <= now)
	{
		return process->child > 0 && process->killed ? 20 : 0;
	}
	int64_t remaining = process->deadline - now;

	/* An EOF/error can precede exit. Reaping must not depend on another
	 * notification; 20 ms polling preserves the synchronous adapter. */
	if (process->output < 0 && remaining > 20)
	{
		remaining = 20;
	}
	return (int)remaining;
}

image_process_status image_process_step(image_process *process)
{
	sigset_t previous;
	unsigned char bytes[IMAGE_PROCESS_CHUNK];
	int64_t now;

	if (!image_process_block(&previous))
	{
		return IMAGE_PROCESS_RUNNING;
	}
	image_process_reap(process);
	now = image_process_now();
	if (now < 0 || (now >= process->deadline &&
	    !(process->exited && process->output < 0)))
	{
		image_process_fail(process, now < 0 ? errno : ETIMEDOUT);
	}
	if (process->error == 0 && process->output >= 0)
	{
		size_t room = process->limit - process->received;
		size_t count = room < sizeof(bytes) ? room + 1 : sizeof(bytes);
		ssize_t received = read(process->output, bytes, count);

		if (received > 0)
		{
			if ((size_t)received > room)
			{
				image_process_fail(process, EFBIG);
			}
			else if (!process->sink(bytes, (size_t)received,
			    process->sink_context))
			{
				image_process_fail(process, errno);
			}
			else
			{
				process->received += (size_t)received;
			}
		}
		else if (received == 0)
		{
			if (!image_process_close(&process->output))
			{
				image_process_fail(process, errno);
			}
		}
		else if (errno != EINTR && errno != EAGAIN &&
		    errno != EWOULDBLOCK)
		{
			image_process_fail(process, errno);
		}
	}
	if (process->error == 0 && process->input >= 0)
	{
		size_t count = process->source->length - process->used;

		if (count == 0)
		{
			if (!image_process_close(&process->input))
			{
				image_process_fail(process, errno);
			}
		}
		else
		{
			if (count > IMAGE_PROCESS_CHUNK)
			{
				count = IMAGE_PROCESS_CHUNK;
			}
			ssize_t sent = fvwm_write_without_sigpipe(
			    process->input, process->source->bytes +
			    process->used, count);

			if (sent > 0)
			{
				process->used += (size_t)sent;
			}
			else if (sent < 0 && errno != EINTR &&
			    errno != EAGAIN && errno != EWOULDBLOCK)
			{
				image_process_fail(process, errno);
			}
		}
	}
	if (process->child < 0 && process->output < 0 &&
	    process->error == 0 &&
	    (!process->exited || !WIFEXITED(process->status) ||
	    WEXITSTATUS(process->status) != 0 ||
	    process->used != process->source->length))
	{
		image_process_fail(process, EIO);
	}
	image_process_status result = process->child > 0 ||
	    (process->error == 0 && process->output >= 0) ?
	    IMAGE_PROCESS_RUNNING : process->error ?
	    IMAGE_PROCESS_FAILED : IMAGE_PROCESS_SUCCEEDED;

	if (result != IMAGE_PROCESS_RUNNING)
	{
		image_process_close(&process->input);
	}
	if (sigprocmask(SIG_SETMASK, &previous, NULL) < 0)
	{
		/* A mask failure cannot justify losing child state. */
		process->error = errno;
		result = process->child > 0 ? IMAGE_PROCESS_RUNNING :
		    IMAGE_PROCESS_FAILED;
	}
	return result;
}

void image_process_cancel(image_process *process)
{
	sigset_t previous;

	if (process != NULL && image_process_block(&previous))
	{
		image_process_fail(process, ECANCELED);
		sigprocmask(SIG_SETMASK, &previous, NULL);
	}
}

int image_process_error(const image_process *process)
{
	return process->error;
}

bool image_process_destroy(image_process *process)
{
	if (process == NULL)
	{
		return true;
	}
	if (process->child > 0)
	{
		errno = EBUSY;
		return false;
	}
	image_process_close(&process->input);
	image_process_close(&process->output);
	free(process);
	return true;
}

bool image_process_handoff(image_process *process)
{
	sigset_t previous;
	struct sigaction disposition = {0};

	if (process == NULL)
	{
		return true;
	}
	if (!image_process_block(&previous))
	{
		return false;
	}
	int queried = sigaction(SIGCHLD, NULL, &disposition);
	int error = queried < 0 ? errno : EINVAL;

	if (queried < 0 || disposition.sa_handler != fvwmReapChildren ||
	    (disposition.sa_flags & SA_NOCLDWAIT))
	{
		sigprocmask(SIG_SETMASK, &previous, NULL);
		errno = error;
		return false;
	}
	image_process_fail(process, ECANCELED);
	if (process->watched)
	{
		fvwm_child_unwatch(process->child);
		process->watched = false;
	}
	/* The reaper will consume any later exit without an owner callback.
	 * Do not retain a numeric PID after this ownership transfer. */
	process->child = -1;
	if (sigprocmask(SIG_SETMASK, &previous, NULL) < 0)
	{
		/* The reaper has no borrowed context. Preserve the mask
		 * error while releasing the detached state. */
		int error = errno;

		free(process);
		errno = error;
		return true;
	}
	free(process);
	return true;
}

bool image_process_run(const image_input *source, const char *program,
	char *const argv[], size_t limit, int timeout_ms, uint64_t memory_limit,
	image_process_sink_fn sink, void *sink_context,
	image_decompress_cancel_fn cancel, void *context)
{
	sigset_t previous;
	struct sigaction previous_child;
	bool action_saved = false;
	bool success = false;
	int error;
	image_process *process;

	if (!image_process_block(&previous))
	{
		return false;
	}
	if (sigaction(SIGCHLD, NULL, &previous_child) < 0)
	{
		goto done;
	}
	/* Preserve normal handlers and pending unrelated child notifications.
	 * Module callers with automatic reaping need temporary waitability. */
	if (previous_child.sa_handler == SIG_IGN ||
	    (previous_child.sa_flags & SA_NOCLDWAIT))
	{
		struct sigaction waitable = previous_child;

		if (waitable.sa_handler == SIG_IGN)
		{
			waitable.sa_handler = SIG_DFL;
		}
		waitable.sa_flags &= ~SA_NOCLDWAIT;
		if (sigaction(SIGCHLD, &waitable, NULL) < 0)
		{
			goto done;
		}
		action_saved = true;
	}
	process = image_process_start(source, program, argv, limit, timeout_ms,
	    memory_limit, sink, sink_context);
	if (process == NULL)
	{
		goto done;
	}
	for (;;)
	{
		struct pollfd endpoints[2];

		if (cancel != NULL && cancel(context))
		{
			image_process_cancel(process);
		}
		image_process_status status = image_process_step(process);

		if (status != IMAGE_PROCESS_RUNNING)
		{
			success = status == IMAGE_PROCESS_SUCCEEDED;
			errno = image_process_error(process);
			break;
		}
		image_process_descriptors(process, endpoints);
		int remaining = image_process_timeout_ms(process);
		int result = poll(endpoints, 2,
		    remaining > 20 ? 20 : remaining);

		if (result < 0 && errno != EINTR)
		{
			int poll_error = errno;

			image_process_cancel(process);
			process->error = poll_error;
		}
	}
	error = errno;
	image_process_destroy(process);
	errno = error;

done:
	error = errno;
	if (action_saved && sigaction(SIGCHLD, &previous_child, NULL) < 0)
	{
		error = errno;
		success = false;
	}
	if (sigprocmask(SIG_SETMASK, &previous, NULL) < 0)
	{
		error = errno;
		success = false;
	}
	errno = error;
	return success;
}
