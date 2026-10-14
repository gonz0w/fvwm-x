/* -*-c-*- */

#ifndef FVWM_BUILTINS_H
#define FVWM_BUILTINS_H

#include "fvwm.h"
#include "screen.h"

void refresh_window(Window w, Bool window_update);
void ApplyDefaultFontAndColors(void);
void InitFvwmDecor(FvwmDecor *decor);
void reset_decor_changes(void);
Bool ReadDecorFace(char *s, DecorFace *df, int button, int verbose);
void FreeDecorFace(Display *dpy, DecorFace *df);
/* Transfer a prepared face, retaining old backing for pending replacements.
 * Empty the source and relocate callbacks to the live destination. */
void replace_decor_face(DecorFace *df, DecorFace *prepared);
/* Return a retained immutable draw snapshot at the window's current scale.
 * Deferred scale leases live with their face, not a transient draw call. */
FvwmPicture *decor_picture_at_scale(FvwmPicture *source,
	monitor_scale_factor factor);
void update_fvwm_colorset(int cset);
void status_send(void);

#endif /* FVWM_BUILTINS_H */
