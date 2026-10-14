#ifndef FVWM_FPROPERTY_H
#define FVWM_FPROPERTY_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
	size_t min_bytes;
	size_t max_bytes;
	int format;
} FPropertyLimits;

typedef enum {
	FPROPERTY_OK,
	FPROPERTY_MISSING,
	FPROPERTY_INVALID,
	FPROPERTY_TOO_LARGE,
	FPROPERTY_X_ERROR
} FPropertyResult;

/* Limits apply to packed wire bytes, including on LP64 Xlib implementations.
 * Outputs are reset on failure. Partial properties are never accepted. */
bool FPropertySizeIsSafe(unsigned long items, int format,
    unsigned long bytes_after, FPropertyLimits limits, size_t *bytes);
FPropertyResult FPropertyRead(Display *display, Window window, Atom property,
    Atom type, FPropertyLimits limits, unsigned char **data, size_t *bytes);

/* Successful data is malloc-owned, with format-32 values packed as uint32_t
 * and an extra terminating zero byte. Free it with free(), not XFree(). */

#define FPROPERTY_TEXT_MAX_BYTES (64U * 1024U)

/* Bounded ICCCM replacements. Text preserves the server encoding and returns
 * Xlib-owned storage. Class strings use XFree; commands use XFreeStringList.
 * Outputs are reset on failure. Names allow STRING/COMPOUND_TEXT/UTF8_STRING. */
Status FGetTextProperty(Display *display, Window window, XTextProperty *text,
    Atom property);
Status FGetWMName(Display *display, Window window, XTextProperty *text);
Status FGetWMIconName(Display *display, Window window, XTextProperty *text);
Status FGetClassHint(Display *display, Window window, XClassHint *hint);
Status FGetCommand(Display *display, Window window, char ***argv, int *argc);

/* Session strings require a nonempty STRING/8 property. Client leaders are
 * exactly one WINDOW/32 value. Both return a safe absent value on rejection. */
char *FGetSessionString(Display *display, Window window, Atom property);
Window FGetClientLeader(Display *display, Window window, Atom property);
Status FGetWMColormapWindows(Display *display, Window window,
    Window **windows, int *count);

#endif
