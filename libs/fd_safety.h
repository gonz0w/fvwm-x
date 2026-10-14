#ifndef FVWMLIB_FD_SAFETY_H
#define FVWMLIB_FD_SAFETY_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

/* Call before exposing descriptors or forking. Failure leaves both at -1. */
bool fvwm_pipe_cloexec(int descriptors[2]);
bool fvwm_fd_set_cloexec(int descriptor, bool enabled);

/* For owners using fd_set: validate its range before protecting inheritance.
 * Failure does not close the descriptor; the caller retains ownership. */
bool fvwm_fd_prepare_select(int descriptor);

/* Owned, private regular file: reject symlinks, hard links and foreign files.
 * Caller owns the returned descriptor. Failure returns -1 and preserves errno. */
int fvwm_open_private_lock(const char *path);

/* Preserve the caller's SIGPIPE disposition, mask and preexisting pending
 * signal; consume only a newly generated signal from an EPIPE write. */
ssize_t fvwm_write_without_sigpipe(int fd, const void *bytes, size_t length);

#endif
