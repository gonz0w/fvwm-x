#ifndef FVWMLIB_ATOM_BATCH_H
#define FVWMLIB_ATOM_BATCH_H

#include "fvwm_x11.h"

/* Intern an independent finite table. On partial failure, retry missing entries
 * once through the original single-name operation; preserve None on failure.
 * This is a server-reply boundary and must precede publication of these atoms.
 */
Bool atom_batch_intern(Display *display, char **names, int count, Atom *atoms);

#endif
