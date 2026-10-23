#ifndef FVWM_APPEARANCE_PREVIEW_H
#define FVWM_APPEARANCE_PREVIEW_H

struct fmodule;
#include "preview_policy.h"
#include "libs/cJSON.h"

cJSON *appearance_preview_request(struct fmodule *owner,
	const struct preview_request *request, const char **error);
/* These touch only an opacity scalar. Safe while interactions borrow faces. */
void appearance_preview_service(void);
int appearance_preview_timeout_ms(void);
void appearance_preview_owner_gone(struct fmodule *owner);

#endif
