/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "fvwm/image_async.h"
#include "fvwm/execcontext.h"
#include "fvwm/fvwm.h"
#include "fvwm/screen.h"
#include "fvwm/externs.h"

#include <assert.h>
#include <stdlib.h>
#include "libs/FRenderInit.h"

void __real_CaptureAllWindows(const exec_context_t *exc);

/* Check normal startup activation; the fixture adds no loader switch. */
void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	assert(image_async_enabled());
	__real_CaptureAllWindows(exc);
}

Bool __real_FRenderGetExtensionSupported(void);

/* The core fault-injection profile exercises its real software fallback. */
Bool __wrap_FRenderGetExtensionSupported(void)
{
	return getenv("FVWM_TEST_COLORSET_SOFTWARE") != NULL ? False :
	    __real_FRenderGetExtensionSupported();
}
