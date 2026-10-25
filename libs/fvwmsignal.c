/* -*-c-*- */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>
 */
/*
 * fvwmsignal.c
 * Written by Chris Rankin,  rankinc@zipworld.com.au
 *
 */

#include "config.h"

#include <sys/wait.h>
#include <setjmp.h>
#include <errno.h>
#include <unistd.h>
#include "fvwmsignal.h"

#define true   1
#define false  0


typedef enum { SIG_INIT=0, SIG_DONE } SIG_STATUS;

volatile sig_atomic_t isTerminated = false;

static volatile sig_atomic_t canJump = false;
_Static_assert(sizeof(sig_atomic_t) >= sizeof(int),
    "child wait status must fit the atomic signal slot");
static volatile sig_atomic_t watched_child;
static volatile sig_atomic_t watched_status;
static volatile sig_atomic_t watched_exited;

int fvwm_child_watch_available(void)
{
	return watched_child == 0;
}

int fvwm_child_watch(pid_t child)
{
	if (child <= 0 || (pid_t)(sig_atomic_t)child != child)
	{
		errno = EINVAL;
		return 0;
	}
	if (!fvwm_child_watch_available())
	{
		errno = EBUSY;
		return 0;
	}
	watched_status = 0;
	watched_exited = 0;
	watched_child = (sig_atomic_t)child;
	return 1;
}

int fvwm_child_status(pid_t child, int *status)
{
	if (watched_child != child || !watched_exited)
	{
		return 0;
	}
	*status = (int)watched_status;
	return 1;
}

void fvwm_child_unwatch(pid_t child)
{
	if (watched_child == child)
	{
		watched_child = 0;
		watched_exited = 0;
	}
}
/* Saved masks belong to each wrapper invocation on supported POSIX hosts. */
static sigjmp_buf deadJump;
/*
 * Reap child processes, preventing them from becoming zombies.
 * We do this asynchronously within the SIGCHLD handler so that
 * "it just happens".
 */
RETSIGTYPE
fvwmReapChildren(int sig)
{
	int saved_errno = errno;
	int status;
	pid_t child;
	(void)sig;

	BSD_BLOCK_SIGNALS;
	/*
	 * This is a signal handler, AND SO MUST BE REENTRANT!
	 * Now the wait() functions are safe here, but please don't
	 * add anything unless you're SURE that the new functions
	 * (plus EVERYTHING they call) are also reentrant. There
	 * are very few functions which are truly safe.
	 */
#if HAVE_WAITPID
	while ((child = waitpid(-1, &status, WNOHANG)) > 0)
	{
		if (child == watched_child)
		{
			watched_status = status;
			watched_exited = 1;
		}
	}
#elif HAVE_WAIT3
	while ((child = wait3(&status, WNOHANG, NULL)) > 0)
	{
		if (child == watched_child)
		{
			watched_status = status;
			watched_exited = 1;
		}
	}
#else
# error One of waitpid or wait3 is needed.
#endif
	BSD_UNBLOCK_SIGNALS;
	errno = saved_errno;

	SIGNAL_RETURN;
}

#ifdef USE_BSD_SIGNALS
static int term_sigs;

/*
 * fvwmSetSignalMask - store the set of mutually exclusive signals
 * away for future reference. This prevents different signals from
 * trying to access the same static data at the same time.
 *
 * NOTE: We don't need this if we have POSIX.1 since we can install
 *       a signal mask automatically using sigaction()
 */
void
fvwmSetSignalMask(int sigmask)
{
	term_sigs = sigmask;
}


/*
 * fvwmGetSignalMask - get the set of signals that will terminate fvwm
 *
 * NOTE: We don't need this if we have POSIX.1 since we can install
 *       a signal mask automatically using sigaction()
 */
int
fvwmGetSignalMask(void)
{
	return term_sigs;
}

#endif


/*
 * fvwmSetTerminate - set the "end-of-execution" flag.
 * This function should ONLY be called at the end of a
 * signal handler. It is an integral part of the mechanism
 * to stop system calls from blocking once the process is
 * finished.
 *
 * NOTE: This is NOT a signal handler function in its own right!
 */
void
fvwmSetTerminate(int sig)
{
	BSD_BLOCK_SIGNALS;

	isTerminated = true;

	if (canJump)
	{
		canJump = false;

		/*
		 * This non-local jump is safe ONLY because we haven't called
		 * any non-reentrant functions in the short period where the
		 * "canJump" variable is true.
		 *
		 * NOTE: No need to restore the signal mask, since siglongjmp
		 *       is designed to do that for us.
		 */
		siglongjmp(deadJump, SIG_DONE);
	}

	BSD_UNBLOCK_SIGNALS;
}


/* Arm only across the termination check and syscall. Callers must prepare
 * their buffers and descriptor arrays before entering these wrappers. */
ssize_t fvwmRead(int fd, void *buffer, size_t count)
{
	volatile ssize_t result = -1;

	errno = EINTR;
	if (sigsetjmp(deadJump, 1) == SIG_INIT)
	{
		canJump = true;
		if (!isTerminated)
		{
			result = read(fd, buffer, count);
		}
		canJump = false;
	}
	return result;
}

ssize_t fvwmWrite(int fd, const void *buffer, size_t count)
{
	volatile ssize_t result = -1;

	errno = EINTR;
	if (sigsetjmp(deadJump, 1) == SIG_INIT)
	{
		canJump = true;
		if (!isTerminated)
		{
			result = write(fd, buffer, count);
		}
		canJump = false;
	}
	return result;
}

int fvwmPoll(struct pollfd *fds, nfds_t count, int timeout_ms)
{
	volatile int result = -1;

	errno = EINTR;
	if (sigsetjmp(deadJump, 1) == SIG_INIT)
	{
		canJump = true;
		if (!isTerminated)
		{
			result = poll(fds, count, timeout_ms);
		}
		canJump = false;
	}
	return result;
}
