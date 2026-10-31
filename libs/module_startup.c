/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "module_startup.h"
#include <errno.h>
#include <signal.h>
#include <unistd.h>

bool module_startup_begin(unsigned int seconds)
{
	struct sigaction action = {0}, previous;
	sigset_t alarm_signal;

	if (!seconds || seconds > MODULE_STARTUP_SECONDS)
		return false;
	action.sa_handler = SIG_DFL;
	if (sigemptyset(&action.sa_mask) ||
	    sigemptyset(&alarm_signal) || sigaddset(&alarm_signal, SIGALRM) ||
	    sigaction(SIGALRM, &action, &previous))
		return false;
	if (sigprocmask(SIG_UNBLOCK, &alarm_signal, NULL))
	{
		int saved_errno = errno;
		(void)sigaction(SIGALRM, &previous, NULL);
		errno = saved_errno;
		return false;
	}
	(void)alarm(seconds);
	return true;
}

void module_startup_finish(void)
{
	(void)alarm(0);
}
