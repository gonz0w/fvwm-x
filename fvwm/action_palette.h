#ifndef FVWM_ACTION_PALETTE_H
#define FVWM_ACTION_PALETTE_H

#include "libs/cJSON.h"
#include "palette_policy.h"

struct fmodule;
struct fvwm_query_spec;

cJSON *action_palette_query(struct fmodule *owner,
	const struct fvwm_query_spec *spec, const char **error);
void action_palette_owner_gone(struct fmodule *owner);
/* Copies a validated target; never returns borrowed window/monitor records. */
bool action_palette_target(struct fmodule *owner,
	const struct palette_request *request, struct palette_row *row,
	char id[PALETTE_ID_MAX + 1], const char **error);
/* Commands contain only fixed verbs and core-validated numeric arguments. */
bool action_palette_command(const struct palette_row *row, unsigned int step,
	char *command, size_t size, const char **error);

#endif
