#ifndef FVWM_POLICY_ORIGIN_H
#define FVWM_POLICY_ORIGIN_H

#include "config_origin.h"

#define FVWM_POLICY_ORIGIN_MAX_BYTES (64U * 1024U)

typedef enum {
	FVWM_POLICY_EFFECT_BUILTIN_DEFAULT,
	FVWM_POLICY_EFFECT_APPLIED,
	FVWM_POLICY_EFFECT_PARSE_ERROR
} fvwm_policy_effect;

typedef enum {
	FVWM_POLICY_RETENTION_NOT_APPLICABLE,
	FVWM_POLICY_RETENTION_RETAINED,
	FVWM_POLICY_RETENTION_UNAVAILABLE,
	FVWM_POLICY_RETENTION_BUDGET_EXHAUSTED
} fvwm_policy_retention;

typedef struct {
	const fvwm_config_origin *source;
	size_t source_charge;
	fvwm_policy_effect effect;
	fvwm_policy_retention retention;
} fvwm_policy_origin;

#define FVWM_POLICY_ORIGIN_INIT { \
	.source = NULL, .source_charge = 0, \
	.effect = FVWM_POLICY_EFFECT_BUILTIN_DEFAULT, \
	.retention = FVWM_POLICY_RETENTION_NOT_APPLICABLE }

/* Call only after the aggregate value changes, with applied/parse-error effect.
 * Retain before releasing the old owner, including identical references. */
void fvwm_policy_origin_changed(fvwm_policy_origin *slot,
	const fvwm_config_origin *source, fvwm_policy_effect effect);
void fvwm_policy_origin_clear(fvwm_policy_origin *slot);

#endif
