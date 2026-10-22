#ifndef FVWM_QUERY_JSON_H
#define FVWM_QUERY_JSON_H

#include "libs/Bindings.h"
#include "libs/cJSON.h"
#include "libs/Colorset.h"
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

struct MenuStyle;
struct FvwmDecor;

/* Configuration activity only; no rendered/effective-window-state claim. */
void fvwm_query_appearance_changed(void);
bool fvwm_query_appearance_token(
	char output[MODULE_QUERY_GENERATION_LEN + 1]);
cJSON *fvwm_query_appearance(const char *generation,
	const struct fvwm_query_spec *spec, const char **error);
cJSON *fvwm_query_menu_styles(const struct MenuStyle *styles,
	const char *generation, const struct fvwm_query_spec *spec,
	const char **error);
cJSON *fvwm_query_decorations(const struct FvwmDecor *decorations,
	const char *generation, const struct fvwm_query_spec *spec,
	const char **error);

struct fvwm_query_colorset {
	int id;
	const colorset_t *colorset;
	const char *update_state;
};

/* Opaque observation identity, shared core incarnation and owner revision. */
bool fvwm_query_token(uint64_t revision,
	char output[MODULE_QUERY_GENERATION_LEN + 1]);
cJSON *fvwm_query_colorsets(const struct fvwm_query_colorset *entries,
	size_t count, size_t total, const char *generation,
	const struct fvwm_query_spec *spec, const char **error);

/* Fixed internal grammar; no expansion or retained caller-owned pointers. */
bool fvwm_query_parse(const char *text, struct fvwm_query_spec *spec);
cJSON *fvwm_query_topology(const monitor_topology_snapshot *snapshot,
	const char **error);
cJSON *fvwm_query_workspaces(struct monitors *monitors, int width, int height,
	const char *mode, const char **error);
cJSON *fvwm_query_binding_policy(const char *generation,
	const struct fvwm_query_spec *spec, unsigned int ignored_modifiers,
	const char **error);
cJSON *fvwm_query_placement(const char *generation,
	const struct fvwm_query_spec *spec, unsigned int drag,
	unsigned int interactive, const char **error);
cJSON *fvwm_query_bindings(const Binding *bindings, const char *generation,
	const struct fvwm_query_spec *spec, const char **error);

#endif
