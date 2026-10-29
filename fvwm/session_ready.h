#ifndef FVWM_SESSION_READY_H
#define FVWM_SESSION_READY_H

/* Remove private environment immediately. Return the validated owned fd or -1.
 * A refused descriptor remains subject to the core's inherited-fd close loop. */
int session_ready_take(void);

/* One nonblocking byte, SIGPIPE-safe, then close and reset to -1. */
void session_ready_publish(int *descriptor);

#endif
