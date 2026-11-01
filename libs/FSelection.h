#ifndef FVWM_FSELECTION_H
#define FVWM_FSELECTION_H

#include <X11/Xlib.h>
#include <stdbool.h>
#include <stddef.h>

#define FSELECTION_TEXT_MAX_BYTES (32U * 1024U)
#define FSELECTION_TIMEOUT_MS 1000

/* Module-only synchronous selection request. Returns malloc-owned complete
 * format-8 text, or NULL on refusal, timeout or invalid data. A private
 * requestor window isolates late replies; unrelated events remain queued.
 * Xlib still requires a responsive X server. Never call from the WM loop. */
char *FSelectionRequestText(Display *display, Atom selection, Atom target,
    Atom property, Time time, int timeout_ms);

/* Optional module service runs without consuming unrelated X events. False
 * cancels the owned request. It must not reenter this request's reader. */
typedef bool (*FSelectionService)(void *context);
char *FSelectionRequestTextWithService(Display *display, Atom selection,
    Atom target, Atom property, Time time, int timeout_ms,
    FSelectionService service, void *context);

/* Additive module-only transport. Existing wrappers above keep their legacy
 * direct-text behavior. No API here may run from the WM event loop. */
typedef enum {
	FSELECTION_OK,
	FSELECTION_REFUSED,
	FSELECTION_INVALID,
	FSELECTION_TOO_LARGE,
	FSELECTION_TIMED_OUT,
	FSELECTION_CANCELLED,
	FSELECTION_X_ERROR
} FSelectionStatus;

typedef struct {
	unsigned char *data;
	size_t length;
	Atom type;
} FSelectionResult;

/* Outputs are initialized even on invalid input; only OK returns data. That
 * data is malloc-owned (free(), not XFree()), with an extra local zero byte.
 * Free resets all fields; do not reuse a populated result without freeing it.
 * Exact preserves format-8 wire bytes, including NULs; it never probes TARGETS.
 * Text negotiates UTF8_STRING then STRING; it rejects embedded NUL, removes
 * one sole trailing wire NUL, and validates UTF-8 when that type is chosen.
 * Refusal alone permits a STRING fallback, advertised after valid TARGETS or
 * guessed after TARGETS refusal. Invalid replies never trigger fallback.
 *
 * Data uses direct or INCR reads; TARGETS uses packed ATOM/32 under a
 * 256-atom/1024-byte cap, including INCR. Text/exact wire data is capped at
 * 32 KiB, excluding the local terminator. timeout_ms must be
 * 1..1000; one absolute deadline covers all attempts, INCR and servicing. Each
 * attempt has a fresh private requestor. Transfer events run before service,
 * at most 32 records per pump; unrelated events remain queued in FIFO order.
 * Service must bound its own work, leave this requestor's protocol events
 * queued and never reenter its reader. False cancels. Neither an individual
 * Xlib call nor an entered callback can be preempted: a responsive server
 * and callback are still required.
 */
FSelectionStatus FSelectionRequestExact(Display *display, Atom selection,
    Atom target, Atom property, Time time, int timeout_ms,
    FSelectionService service, void *context, FSelectionResult *result);
FSelectionStatus FSelectionRequestTextNegotiated(Display *display,
    Atom selection, Atom property, Time time, int timeout_ms,
    FSelectionService service, void *context, FSelectionResult *result);
void FSelectionResultFree(FSelectionResult *result);

#endif
