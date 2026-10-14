/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include "atom_batch.h"
#include "log.h"

Bool atom_batch_intern(Display *display, char **names, int count, Atom *atoms)
{
	Bool complete = True;

	if (count <= 0)
	{
		return count == 0;
	}

	for (int i = 0; i < count; i++)
	{
		atoms[i] = None;
	}
	if (XInternAtoms(display, names, count, False, atoms))
	{
		return True;
	}
	/* Keep successful results and bound retry work to missing entries. */
	for (int i = 0; i < count; i++)
	{
		if (atoms[i] == None)
		{
			atoms[i] = XInternAtom(display, names[i], False);
			if (atoms[i] == None)
			{
				complete = False;
			}
		}
	}
	if (!complete)
	{
		fvwm_debug(__func__, "Unable to intern all requested atoms");
	}
	return complete;
}
