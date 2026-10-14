#ifndef FVWM_FSELECTION_H
#define FVWM_FSELECTION_H

#include <X11/Xlib.h>
#include <stdbool.h>

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

#endif
