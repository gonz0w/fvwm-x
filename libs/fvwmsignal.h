/* -*-c-*- */
#ifndef FVWMLIB_FVWMSIGNAL_H
#define FVWMLIB_FVWMSIGNAL_H

/* This module provides wrappers around system functions that could
 * potentially block: read(), write() and poll(). They check that
 * the "terminate" flag is still clear and then call the system
 * function in one atomic operation. This ensures that fvwm will not
 * block in a system function once it has received the signal to quit.
 *
 * This module was written by Chris Rankin,  rankinc@zipworld.com.au
 */
/* Supported hosts use POSIX.1 signal semantics. */

#include <signal.h>
#include <poll.h>
#include <sys/types.h>
#include "ftime.h"
#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

/*
 * Global variables
 */
extern volatile sig_atomic_t isTerminated;


/*
 * Module prototypes
 */
RETSIGTYPE fvwmReapChildren(int sig);
/* One owned asynchronous child. Call these with SIGCHLD blocked. The reaper
 * records only atomic PID/status values; it never calls an owner callback. */
int fvwm_child_watch_available(void);
int fvwm_child_watch(pid_t child);
int fvwm_child_status(pid_t child, int *status);
void fvwm_child_unwatch(pid_t child);
extern void fvwmSetTerminate(int sig);
extern int fvwmPoll(struct pollfd *fds, nfds_t count, int timeout_ms);
/* Read with the same atomic termination check as fvwmPoll. */
extern ssize_t fvwmRead(int fd, void *buffer, size_t count);
/* Write with the same atomic termination check as fvwmRead. */
extern ssize_t fvwmWrite(int fd, const void *buffer, size_t count);

#endif /* FVWMLIB_FVWMSIGNAL_H */
