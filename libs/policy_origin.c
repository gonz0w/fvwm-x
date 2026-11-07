/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "policy_origin.h"

void fvwm_policy_origin_clear(fvwm_policy_origin *slot)
{
	fvwm_config_origin_release(slot->source);
	*slot = (fvwm_policy_origin)FVWM_POLICY_ORIGIN_INIT;
}

void fvwm_policy_origin_changed(fvwm_policy_origin *slot,
	const fvwm_config_origin *source, fvwm_policy_effect effect)
{
	fvwm_policy_origin next = FVWM_POLICY_ORIGIN_INIT;
	size_t charge = fvwm_config_origin_storage_bytes(source);

	next.effect = effect;
	if (charge > FVWM_POLICY_ORIGIN_MAX_BYTES)
		next.retention = FVWM_POLICY_RETENTION_BUDGET_EXHAUSTED;
	else if (source == NULL)
		next.retention = FVWM_POLICY_RETENTION_UNAVAILABLE;
	else
	{
		next.source = fvwm_config_origin_retain(source);
		next.source_charge = charge;
		next.retention = FVWM_POLICY_RETENTION_RETAINED;
	}
	fvwm_policy_origin_clear(slot);
	*slot = next;
}
