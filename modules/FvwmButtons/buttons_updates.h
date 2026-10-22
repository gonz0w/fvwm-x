#ifndef BUTTONS_UPDATES_H
#define BUTTONS_UPDATES_H

#include <stdbool.h>
#include <stddef.h>

#define BUTTONS_UPDATE_BYTES 8192
#define BUTTONS_UPDATE_GROUPS 32
#define BUTTONS_UPDATE_CHANGES 128

typedef enum
{
	BUTTONS_TITLE, BUTTONS_ICON, BUTTONS_ACTIVE_TITLE, BUTTONS_ACTIVE_ICON,
	BUTTONS_PRESS_TITLE, BUTTONS_PRESS_ICON, BUTTONS_COLORSET,
	BUTTONS_ENABLED, BUTTONS_SELECTED, BUTTONS_ACTIVE_COLORSET,
	BUTTONS_PRESS_COLORSET, BUTTONS_FOCUS_COLORSET, BUTTONS_DISABLED_COLORSET,
	BUTTONS_UPDATE_OPTIONS
} buttons_update_option;

typedef struct
{
	buttons_update_option option;
	char *value;
} buttons_update_change;

typedef struct
{
	char *selector;
	size_t first, count;
} buttons_update_group;

/* All token pointers belong to this operation's storage. No module state is
 * touched by parsing, and a refused plan must not be applied. */
typedef struct
{
	char storage[BUTTONS_UPDATE_BYTES + 1];
	buttons_update_group groups[BUTTONS_UPDATE_GROUPS];
	buttons_update_change changes[BUTTONS_UPDATE_CHANGES];
	size_t group_count, change_count;
} buttons_update_plan;

bool buttons_parse_updates(const char *line, buttons_update_plan *plan,
	const char **error);

#endif
