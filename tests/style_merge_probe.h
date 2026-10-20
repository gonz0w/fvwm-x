#ifndef FVWM_TEST_STYLE_MERGE_PROBE_H
#define FVWM_TEST_STYLE_MERGE_PROBE_H

#include "fvwm/execcontext.h"
#include "fvwm/style.h"
#include "fvwm/style_id.h"

void style_merge_visit(const style_id_t *id, unsigned bucket);
void style_merge_probe(const exec_context_t *exc, FvwmWindow *window,
	void (*lookup)(FvwmWindow *, window_style *, Bool),
	void (*distribution)(unsigned));

#endif
