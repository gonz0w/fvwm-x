#ifndef FVWM_APPEARANCE_PREVIEW_H
#define FVWM_APPEARANCE_PREVIEW_H

struct fmodule;
#include "preview_policy.h"
#include "libs/cJSON.h"

cJSON *appearance_preview_request(struct fmodule *owner,
	const struct preview_request *request, const char **error);
/* Interaction-safe service: scalar recovery, or marking resource work pending.
 * Resource owners are never replaced or released from these service paths. */
void appearance_preview_service(void);
int appearance_preview_timeout_ms(void);
void appearance_preview_owner_gone(struct fmodule *owner);
/* Resource publication, recovery and release require the safe outer boundary. */
void appearance_preview_outer_boundary(void);
bool appearance_preview_outer_pending(void);
cJSON *appearance_preview_capability(void);

#endif
