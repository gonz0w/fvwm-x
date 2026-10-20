#ifndef FVWM_CONDITION_NAMES_H
#define FVWM_CONDITION_NAMES_H

#include "libs/fvwm_x11.h"

/* Window name data structure for window conditions: a list of lists
   of names to match, the boolean operation on the matches being an
   AND of ORs. */
struct namelist			/* matches to names in this list are ORed */
{
	char *name;
	struct namelist *next;
};

struct name_condition		/* matches to namelists in this list are
				   ANDed, after possibly inverting each */
{
	Bool invert;
	struct namelist *namelist;
	struct name_condition *next;
};

#endif
