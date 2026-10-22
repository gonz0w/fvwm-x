#ifndef FVWM_QUERY_JSON_H
#define FVWM_QUERY_JSON_H

#include "libs/Bindings.h"
#include "libs/cJSON.h"
#include "libs/monitor_types.h"
#include "libs/module_query.h"
#include <stdint.h>

struct fvwm_query_spec {
	char cookie[65];
	char serial[17];
	char resource[16];
	unsigned int offset;
	unsigned int limit;
	char generation[MODULE_QUERY_GENERATION_LEN + 1];
};

/* Fixed internal grammar; no expansion or retained caller-owned pointers. */
bool fvwm_query_parse(const char *text, struct fvwm_query_spec *spec);
cJSON *fvwm_query_topology(const monitor_topology_snapshot *snapshot,
	const char **error);
cJSON *fvwm_query_workspaces(struct monitors *monitors, int width, int height,
	const char *mode, const char **error);
cJSON *fvwm_query_bindings(const Binding *bindings, const char *generation,
	const struct fvwm_query_spec *spec, const char **error);

#endif
