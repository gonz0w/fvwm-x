/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "libs/Flocale.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Non-installed fixture: exercise a partially prepared font transaction using
 * the exact production Form objects. No environment hook is added to Form. */
FlocaleFont *__real_FlocaleLoadFontAtScale(Display *, char *, char *,
	monitor_scale_factor, unsigned int);

FlocaleFont *__wrap_FlocaleLoadFontAtScale(Display *display, char *name,
	char *module, monitor_scale_factor factor, unsigned int generation)
{
	static unsigned int acquisitions;
	static bool refused;
	const char *arm = getenv("FVWM_TEST_FORM_SCALE_FAIL_FILE");

	/* Keep the unavailable-font condition stable until the owning fixture
	 * removes its arm file. A real ConfigureNotify may otherwise retry before
	 * the observer can inspect the retained complete rendering. */
	if (factor.quarters != MONITOR_SCALE_QUARTERS_MIN &&
	    arm != NULL && access(arm, F_OK) == 0)
	{
		if (refused)
			return NULL;
		if (++acquisitions != 2)
			return __real_FlocaleLoadFontAtScale(display, name, module,
				factor, generation);
		char path[4096];
		int length = snprintf(path, sizeof(path), "%s.failed", arm);
		FILE *receipt;

		refused = true;
		if (length > 0 && (size_t)length < sizeof(path) &&
		    (receipt = fopen(path, "w")) != NULL)
		{
			fprintf(receipt, "{\"pid\":%ld,\"quarters\":%u,"
				"\"attempt\":%u}\n", (long)getpid(),
				factor.quarters, acquisitions);
			fclose(receipt);
		}
		return NULL;
	}
	return __real_FlocaleLoadFontAtScale(display, name, module, factor,
		generation);
}
