#ifndef FVWM_ROOT_BACKGROUND_H
#define FVWM_ROOT_BACKGROUND_H
#include "libs/picture_async.h"
#include "libs/background_plan.h"
#include "preview_policy.h"

struct root_background;
void root_background_select(void);
void root_background_outer_boundary(void);
bool root_background_outer_pending(void);
bool root_background_owned(void);
bool root_background_validate(void);
struct root_background *root_background_capture(void);
struct root_background *root_background_prepare(
	const struct background_plan *plan);
FvwmPicture *root_background_picture(const struct root_background *root);
bool root_background_ready(const struct root_background *root);
bool root_background_failed(const struct root_background *root);
bool root_background_current(const struct root_background *root);
bool root_background_publishable(const struct root_background *root);
/* Stage retained ownership before the short server grab. READY must be
 * paired with end, including a later menu-identity refusal. RETRY and
 * CONFLICT release their grab and staging before returning. */
enum preview_publication_result root_background_begin_move(
	struct root_background *target);
void root_background_end_move(void);
enum preview_publication_result root_background_publish(
	struct root_background *root, bool dependents);
void root_background_release(struct root_background *root);
/* Exactly one expected property notification is consumed for each request. */
bool root_background_notification(const XPropertyEvent *event);
void root_background_shutdown(void);
#endif
