#ifndef FVWM_INTERACTION_H
#define FVWM_INTERACTION_H

#include "libs/fvwm_x11.h"
#include <stdint.h>

#define FVWM_INTERACTION_MOTION_BUDGET 32
#define FVWM_BUTTON_RELEASE_INTERVAL_MS 20
#define FVWM_BUTTON_RELEASE_INTERVAL_NS \
	(UINT64_C(1000000) * FVWM_BUTTON_RELEASE_INTERVAL_MS)

typedef enum
{
	FVWM_INTERACTION_RUNNING,
	FVWM_INTERACTION_TARGET_GONE,
	FVWM_INTERACTION_TERMINATED,
	FVWM_INTERACTION_IO_ERROR
} fvwm_interaction_result;

/* IDs only: cancellation leaves lifecycle events queued until the command
 * stack unwinds. No event handler may free the drag's borrowed window here. */
typedef struct
{
	Window client;
	Window root;
	Bool can_withdraw;
	fvwm_interaction_result result;
	/* Optional bounded transport/command service; no X event dispatch. */
	Bool (*service)(int timeout_ms);
} fvwm_interaction;

Bool fvwm_interaction_target_event(
	const fvwm_interaction *interaction, const XEvent *event);
Bool fvwm_interaction_cancelled(fvwm_interaction *interaction);
Bool fvwm_interaction_service(fvwm_interaction *interaction, int timeout_ms);
Bool fvwm_interaction_wait(
	fvwm_interaction *interaction, long mask, XEvent *event);
void fvwm_interaction_coalesce(long mask, XEvent *event);

/* Unsupported button numbers have no mask; validate before shifting. */
unsigned int fvwm_interaction_button_mask(unsigned int button);

/* Invalid release numbers retain the previous button state. Keyboard
 * modifiers are never part of the physical release barrier. */
unsigned int fvwm_button_release_mask(
	unsigned int previous, unsigned int button, unsigned int event_state);
/* Ceil the remaining monotonic interval for finite readiness waits. A clock
 * failure or regression keeps the wait finite without querying in a spin. */
int fvwm_button_release_timeout(uint64_t previous_ns, uint64_t now_ns);

#endif
