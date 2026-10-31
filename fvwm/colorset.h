/* -*-c-*- */

#ifndef FVWM_COLORSET_H
#define FVWM_COLORSET_H

/* ---------------------------- included header files ---------------------- */
#include "libs/fvwm_x11.h"
#include "libs/module_query.h"
#include <stdbool.h>
#include <stdint.h>
#include "libs/background_plan.h"
#include "libs/PictureBase.h"
#include "preview_policy.h"

/* ---------------------------- global definitions ------------------------- */

/* ---------------------------- global macros ------------------------------ */

/* ---------------------------- type definitions --------------------------- */

/* ---------------------------- forward declarations ----------------------- */

/* ---------------------------- exported variables (globals) --------------- */

/* ---------------------------- interface functions ------------------------ */

bool colorset_query_token(char output[MODULE_QUERY_GENERATION_LEN + 1]);
bool colorset_root_plan(struct background_plan *plan);
uint64_t colorset_root_revision(void);
void colorset_root_owned_publish(FvwmPicture *picture, bool dependents);
void colorset_root_owned_refresh(void);
struct colorset_preview;
/* One private, fixed-role bundle. Moves and destruction are outer-only. */
struct colorset_preview *colorset_preview_prepare(
	const uint32_t colors[14][4], const char **error);
struct colorset_preview *colorset_preview_prepare_complete(
	const uint32_t colors[14][4], const struct background_plan *plan,
	const char **error);
bool colorset_preview_ready(struct colorset_preview *bundle, const char **error);
bool colorset_preview_can_move(struct colorset_preview *bundle);
enum preview_publication_result colorset_preview_begin_move(
	struct colorset_preview *bundle);
void colorset_preview_end_move(struct colorset_preview *bundle);
bool colorset_preview_current(struct colorset_preview *bundle);
enum preview_publication_result colorset_preview_validate(
	struct colorset_preview *bundle);
bool colorset_preview_work_pending(struct colorset_preview *bundle);
void colorset_preview_move(struct colorset_preview *bundle);
void colorset_preview_broadcast(const struct colorset_preview *bundle);
void colorset_preview_release(struct colorset_preview *bundle);
const char *colorset_query_update_state(int id);
void parse_colorset(int n, char *line);
void cleanup_colorsets(void);
Bool alloc_colorset(int n);
void update_root_transparent_colorset(Atom prop);

#endif /* FVWM_COLORSET_H */
