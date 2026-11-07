/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/policy_origin.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static bool override_charge;
static size_t injected_charge;

size_t __real_fvwm_config_origin_storage_bytes(
	const fvwm_config_origin *source);
size_t __wrap_fvwm_config_origin_storage_bytes(const fvwm_config_origin *source)
{
	return override_charge ? injected_charge :
		__real_fvwm_config_origin_storage_bytes(source);
}

static void assert_builtin(const fvwm_policy_origin *slot)
{
	assert(slot->source == NULL && slot->source_charge == 0);
	assert(slot->effect == FVWM_POLICY_EFFECT_BUILTIN_DEFAULT);
	assert(slot->retention == FVWM_POLICY_RETENTION_NOT_APPLICABLE);
}

static void test_owned_slots(void)
{
	fvwm_policy_origin slots[3] = {FVWM_POLICY_ORIGIN_INIT,
		FVWM_POLICY_ORIGIN_INIT, FVWM_POLICY_ORIGIN_INIT};
	fvwm_config_origin *root = fvwm_config_origin_file("/root.cfg", 2, 4,
		NULL);
	fvwm_config_origin *child = fvwm_config_origin_file("/child.cfg", 8, 9,
		root);
	size_t charge = fvwm_config_origin_storage_bytes(child);

	for (size_t i = 0; i < 3; i++)
	{
		assert_builtin(&slots[i]);
		fvwm_policy_origin_changed(&slots[i], child,
			FVWM_POLICY_EFFECT_APPLIED);
		assert(slots[i].source_charge == charge);
		assert(slots[i].retention == FVWM_POLICY_RETENTION_RETAINED);
	}
	/* Closing Read frames cannot invalidate any of the three owners. */
	fvwm_config_origin_release(root);
	fvwm_config_origin_release(child);
	for (size_t i = 0; i < 3; i++)
	{
		assert(strstr(fvwm_config_origin_json(slots[i].source),
			"\"include\":[{\"path\":\"/root.cfg\","
			"\"first\":2,\"last\":4}]"));
		/* Replace while only the slot itself owns the source. */
		fvwm_policy_origin_changed(&slots[i], slots[i].source,
			FVWM_POLICY_EFFECT_PARSE_ERROR);
		assert(slots[i].source_charge == charge);
		assert(slots[i].effect == FVWM_POLICY_EFFECT_PARSE_ERROR);
		fvwm_policy_origin_clear(&slots[i]);
		assert_builtin(&slots[i]);
		fvwm_policy_origin_clear(&slots[i]);
		assert_builtin(&slots[i]);
	}
}

static void test_budget_and_unavailable(void)
{
	fvwm_policy_origin slot = FVWM_POLICY_ORIGIN_INIT;
	fvwm_config_origin *source = fvwm_config_origin_file("/budget.cfg",
		1, 1, NULL);
	fvwm_config_origin *bad = fvwm_config_origin_file("relative", 1, 1,
		NULL);

	fvwm_policy_origin_changed(&slot, NULL, FVWM_POLICY_EFFECT_APPLIED);
	assert(slot.effect == FVWM_POLICY_EFFECT_APPLIED);
	assert(slot.retention == FVWM_POLICY_RETENTION_UNAVAILABLE);
	assert(slot.source == NULL && slot.source_charge == 0);
	fvwm_policy_origin_changed(&slot, bad, FVWM_POLICY_EFFECT_PARSE_ERROR);
	assert(slot.effect == FVWM_POLICY_EFFECT_PARSE_ERROR);
	assert(slot.retention == FVWM_POLICY_RETENTION_RETAINED);
	assert(strstr(fvwm_config_origin_json(slot.source), "unavailable"));
	fvwm_config_origin_release(bad);

	/* Inject complete allocation charges: current serialized-origin bounds
	 * are tighter, but future source layouts must still obey this cap. */
	override_charge = true;
	injected_charge = FVWM_POLICY_ORIGIN_MAX_BYTES;
	fvwm_policy_origin_changed(&slot, source, FVWM_POLICY_EFFECT_APPLIED);
	assert(slot.retention == FVWM_POLICY_RETENTION_RETAINED);
	assert(slot.source_charge == FVWM_POLICY_ORIGIN_MAX_BYTES);
	injected_charge++;
	fvwm_policy_origin_changed(&slot, source, FVWM_POLICY_EFFECT_APPLIED);
	assert(slot.retention == FVWM_POLICY_RETENTION_BUDGET_EXHAUSTED);
	assert(slot.source == NULL && slot.source_charge == 0);
	injected_charge = SIZE_MAX;
	fvwm_policy_origin_changed(&slot, source,
		FVWM_POLICY_EFFECT_PARSE_ERROR);
	assert(slot.retention == FVWM_POLICY_RETENTION_BUDGET_EXHAUSTED);
	assert(slot.effect == FVWM_POLICY_EFFECT_PARSE_ERROR);
	assert(slot.source == NULL && slot.source_charge == 0);
	override_charge = false;
	fvwm_policy_origin_changed(&slot, source, FVWM_POLICY_EFFECT_APPLIED);
	assert(slot.retention == FVWM_POLICY_RETENTION_RETAINED);
	assert(slot.source_charge == fvwm_config_origin_storage_bytes(source));
	fvwm_config_origin_release(source);
	assert(strstr(fvwm_config_origin_json(slot.source), "/budget.cfg"));
	fvwm_policy_origin_clear(&slot);
	assert_builtin(&slot);
}

int main(void)
{
	test_owned_slots();
	test_budget_and_unavailable();
	return 0;
}
