/* -*-c-*- */

#ifndef FVWM_COLORSET_H
#define FVWM_COLORSET_H

/* ---------------------------- included header files ---------------------- */
#include "libs/fvwm_x11.h"
#include "libs/module_query.h"
#include <stdbool.h>
#include <stdint.h>

/* ---------------------------- global definitions ------------------------- */

/* ---------------------------- global macros ------------------------------ */

/* ---------------------------- type definitions --------------------------- */

/* ---------------------------- forward declarations ----------------------- */

/* ---------------------------- exported variables (globals) --------------- */

/* ---------------------------- interface functions ------------------------ */

bool colorset_query_token(char output[MODULE_QUERY_GENERATION_LEN + 1]);
struct colorset_preview;
/* One private, fixed-role bundle. Moves and destruction are outer-only. */
struct colorset_preview *colorset_preview_prepare(
	const uint32_t colors[14][4], const char **error);
void colorset_preview_move(struct colorset_preview *bundle);
void colorset_preview_broadcast(void);
void colorset_preview_release(struct colorset_preview *bundle);
const char *colorset_query_update_state(int id);
void parse_colorset(int n, char *line);
void cleanup_colorsets(void);
Bool alloc_colorset(int n);
void update_root_transparent_colorset(Atom prop);

#endif /* FVWM_COLORSET_H */
