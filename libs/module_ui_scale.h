#ifndef FVWM_MODULE_UI_SCALE_H
#define FVWM_MODULE_UI_SCALE_H

#include "libs/module_query.h"
#include "libs/monitor_ownership.h"
#include "libs/monitor_scale.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MODULE_UI_SCALE_MAX MODULE_QUERY_MONITORS_MAX
#define MODULE_UI_SCALE_NAME_MAX MODULE_QUERY_NAME_MAX
#define MODULE_UI_SCALE_LINE_MAX 768

typedef struct
{
	Atom name_atom;
	unsigned int quarters;
	bool primary;
	rectangle rect;
	char name[MODULE_UI_SCALE_NAME_MAX + 1];
} module_ui_scale_row;

typedef struct
{
	uint64_t epoch;
	unsigned int count;
	bool root_fallback;
	module_ui_scale_row rows[MODULE_UI_SCALE_MAX];
} module_ui_scale_snapshot;

typedef struct
{
	module_ui_scale_snapshot published;
	module_ui_scale_snapshot staging;
	unsigned int received;
	bool pending;
} module_ui_scale_receiver;

typedef enum
{
	MODULE_UI_SCALE_IGNORED,
	MODULE_UI_SCALE_STAGED,
	MODULE_UI_SCALE_PUBLISHED,
	MODULE_UI_SCALE_UNCHANGED,
	MODULE_UI_SCALE_REFUSED
} module_ui_scale_status;

typedef struct
{
	unsigned int count;
	char lines[MODULE_UI_SCALE_MAX + 2][MODULE_UI_SCALE_LINE_MAX];
} module_ui_scale_bundle;

/* MonitorUI 1 BEGIN EPOCH COUNT ROOT_FALLBACK
 * MonitorUI 1 ROW EPOCH INDEX ATOM QUARTERS PRIMARY X Y WIDTH HEIGHT NAME_HEX
 * MonitorUI 1 END EPOCH COUNT
 * EPOCH is exactly 16 lowercase hexadecimal digits, nonzero. INDEX is
 * zero-based and sequential. NAME_HEX encodes 1..256 non-NUL bytes. Numeric
 * fields are strict decimals; geometry is actual root pixels. */
bool module_ui_scale_validate(const module_ui_scale_snapshot *snapshot);
/* On failure count is zero; no partial bundle may be sent. */
bool module_ui_scale_format(const module_ui_scale_snapshot *snapshot,
	module_ui_scale_bundle *bundle);
/* Zero-initialize the receiver. Only a complete END publishes; exact same-
 * epoch replay is UNCHANGED, conflicting replay is refused. A same/newer BEGIN
 * replaces incomplete staging. Unrelated config is IGNORED. Refused/lost
 * updates never replace the last good publication. */
module_ui_scale_status module_ui_scale_receive(module_ui_scale_receiver *state,
	const char *text);
/* End-of-config, module loss or an owner deadline aborts only pending data. */
void module_ui_scale_abort(module_ui_scale_receiver *state);
/* Returns a borrowed row from snapshot, or NULL for invalid/unavailable state.
 * Selection uses the existing monitor ownership policy and hysteresis. */
const module_ui_scale_row *module_ui_scale_select(
	const module_ui_scale_snapshot *snapshot, const rectangle *window,
	Atom incumbent_atom);

#endif
