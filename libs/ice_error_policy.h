#ifndef FVWMLIB_ICE_ERROR_POLICY_H
#define FVWMLIB_ICE_ERROR_POLICY_H

#include <stddef.h>
#include "FSMlib.h"

/* Reinstallation must retain the saved predecessor, never our own callback. */
static inline FIceIOErrorHandler fvwm_ice_error_previous(
	FIceIOErrorHandler handler, FIceIOErrorHandler previous,
	FIceIOErrorHandler candidate, FIceIOErrorHandler default_handler)
{
	if (candidate == handler)
	{
		return previous;
	}
	return candidate == default_handler ? NULL : candidate;
}

#endif
