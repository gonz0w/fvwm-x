#ifndef FVWMSCRIPT_SCRIPT_DELAY_H
#define FVWMSCRIPT_SCRIPT_DELAY_H

#include <stdbool.h>
#include <time.h>

typedef enum
{
	SCRIPT_DELAY_ERROR = -1,
	SCRIPT_DELAY_ELAPSED,
	SCRIPT_DELAY_CANCELLED
} script_delay_result;

typedef bool (*script_delay_cancel_fn)(void *context);

/* Keep the legacy 16667 microsecond tick without signed multiplication. */
void script_delay_duration(int ticks, struct timespec *duration);
/* Rounded-up remaining time in a one-second periodic interval. */
int script_periodic_timeout_ms(const struct timespec *last,
	const struct timespec *now);
/* Rounded-up remaining time in the total two-second message shutdown grace.
 * The caller retains the same start across input and interruptions. */
int script_shutdown_timeout_ms(const struct timespec *start,
	const struct timespec *now);
/* Inactive periodic tasks never read clock state; pending sessions wake in
 * at most ten milliseconds. */
int script_readiness_timeout_ms(bool periodic, bool session_pending,
	const struct timespec *last, const struct timespec *now);
/* Sleep through signal interruptions; this does not dispatch module events. */
bool script_delay_ticks(int ticks);
/* Check cancellation before waiting and before expiry. For fd >= 0, check
 * must drain readable input or move it to the caller's deferred event queue.
 * Other input and signals never restart the elapsed-time deadline. */
script_delay_result script_delay_wait(int ticks, int fd,
	script_delay_cancel_fn check, void *context);

#endif
