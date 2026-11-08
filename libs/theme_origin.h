#ifndef FVWM_THEME_ORIGIN_H
#define FVWM_THEME_ORIGIN_H

#include "config_origin.h"
#include <stdbool.h>

#define FVWM_THEME_ORIGIN_MAX_BYTES (1024U * 1024U)
#define FVWM_THEME_DONOR_MAX_BYTES 256U

typedef enum {
	FVWM_THEME_BUILTIN_DEFAULT, FVWM_THEME_MERGE, FVWM_THEME_COPY,
	FVWM_THEME_RESET, FVWM_THEME_PREVIEW
} fvwm_theme_operation;

typedef enum {
	FVWM_THEME_NOT_APPLICABLE, FVWM_THEME_RETAINED,
	FVWM_THEME_UNAVAILABLE, FVWM_THEME_BUDGET_EXHAUSTED
} fvwm_theme_retention;

typedef struct {
	const fvwm_config_origin *source;
	char *copied_from;
	size_t charge;
	fvwm_theme_operation operation;
	fvwm_theme_retention retention;
	bool replaces_definition;
} fvwm_theme_origin;

/* Zero initialization is the builtin-default record. Every slot owns its
 * references; struct assignment is a move only when the old slot is cleared
 * and the moved-from slot is zeroed. Copies, including previews, are charged
 * conservatively for the whole source chain even if snapshots share storage. */
#define FVWM_THEME_ORIGIN_INIT {0}
void fvwm_theme_origin_clear(fvwm_theme_origin *slot);
void fvwm_theme_origin_clone(fvwm_theme_origin *slot,
	const fvwm_theme_origin *origin);
void fvwm_theme_origin_move(fvwm_theme_origin *slot, fvwm_theme_origin *origin);
/* Accepted aggregate operation, not ownership of every field or success of
 * every clause. Rejected no-ops must not call this helper. */
void fvwm_theme_origin_definition(fvwm_theme_origin *slot,
	const fvwm_config_origin *source, fvwm_theme_operation operation,
	const char *donor);
void fvwm_theme_origin_preview(fvwm_theme_origin *slot);
size_t fvwm_theme_origin_retained_bytes(void);

#endif
