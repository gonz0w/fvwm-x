#ifndef FVWM_SESSION_READY_H
#define FVWM_SESSION_READY_H

#include <stdbool.h>

/* Remove private environment immediately. Return the validated owned fd or -1.
 * A refused descriptor remains subject to the inherited-fd close loop. */
int session_ready_take(void);

/* Retain the private, inherited FD4 flock description for the core lifetime.
 * Refusal never unlocks it; the inherited-fd close loop closes refused fds. */
int session_lease_take(void);

/* Retain FD5's private writable lifetime FIFO, accepting BSD O_RDWR ends.
 * Remove its private marker immediately and protect ordinary exec children. */
int session_bus_lifetime_take(void);

/* Only bracket the native Restart exec. Prepare both leases together,
 * rolling both back on failure; ordinary children retain CLOEXEC and no marker. */
bool session_leases_prepare_restart(int lease_descriptor, int bus_descriptor);
void session_leases_finish_restart(int lease_descriptor, int bus_descriptor);

/* One nonblocking write: R, then L for FD4 and B for FD5. SIGPIPE-safe,
 * then close and reset the readiness descriptor to -1. */
void session_ready_publish(int *descriptor, int lease_descriptor,
	int bus_descriptor);

#endif
