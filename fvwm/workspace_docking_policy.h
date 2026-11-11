#ifndef FVWM_WORKSPACE_DOCKING_POLICY_H
#define FVWM_WORKSPACE_DOCKING_POLICY_H

#include <stddef.h>
#include <stdint.h>

#define WORKSPACE_DOCKING_NAME_MAX 256
#define WORKSPACE_DOCKING_CONNECTOR_MAX 64

struct workspace_docking_connector {
	char name[WORKSPACE_DOCKING_NAME_MAX + 1];
	uint64_t role;
};

enum workspace_docking_result {
	WORKSPACE_DOCKING_MATCH,
	WORKSPACE_DOCKING_MISSING,
	WORKSPACE_DOCKING_AMBIGUOUS,
	WORKSPACE_DOCKING_INVALID,
};

/* Resolve a configured NUL-terminated connector name against copied current
 * topology facts. Names compare byte for byte and contain no ASCII controls.
 * Each matching fact must be unique, even if duplicate facts share a role.
 * A match returns a nonzero role token valid only in the supplied topology.
 * Other results clear *role and leave reachable fallback to the caller.
 */
enum workspace_docking_result workspace_docking_resolve(const char *connector,
	const struct workspace_docking_connector *live, size_t count,
	uint64_t *role);

#endif
