/* -*-c-*- */

#ifndef FVWM_COLORSET_H
#define FVWM_COLORSET_H

/* ---------------------------- included header files ---------------------- */
#include "libs/fvwm_x11.h"
#include "libs/module_query.h"
#include <stdbool.h>

/* ---------------------------- global definitions ------------------------- */

/* ---------------------------- global macros ------------------------------ */

/* ---------------------------- type definitions --------------------------- */

/* ---------------------------- forward declarations ----------------------- */

/* ---------------------------- exported variables (globals) --------------- */

/* ---------------------------- interface functions ------------------------ */

bool colorset_query_token(char output[MODULE_QUERY_GENERATION_LEN + 1]);
const char *colorset_query_update_state(int id);
void parse_colorset(int n, char *line);
void cleanup_colorsets(void);
Bool alloc_colorset(int n);
void update_root_transparent_colorset(Atom prop);

#endif /* FVWM_COLORSET_H */
