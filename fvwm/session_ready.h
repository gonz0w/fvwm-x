#ifndef FVWM_SESSION_READY_H
#define FVWM_SESSION_READY_H

#include <stdbool.h>

/* Remove private environment immediately. Return the validated owned fd or -1.
 * A refused descriptor remains subject to the inherited-fd close loop. */
int session_ready_take(void);

/* Retain the private, inherited FD4 flock description for the core lifetime.
 * Refusal never unlocks it; the inherited-fd close loop closes refused fds. */
int session_lease_take(void);

/* Only bracket the native Restart exec. Ordinary children retain CLOEXEC and
 * no private environment. A failed preparation restores that normal state. */
bool session_lease_prepare_restart(int descriptor);
void session_lease_finish_restart(int descriptor);

/* One nonblocking write: RL with a retained lease, otherwise R. SIGPIPE-safe,
 * then close and reset the readiness descriptor to -1. */
void session_ready_publish(int *descriptor, int lease_descriptor);

#endif
