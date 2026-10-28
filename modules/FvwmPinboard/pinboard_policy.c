/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "pinboard_policy.h"
#include <limits.h>
#include <string.h>
#include <strings.h>

bool pinboard_number(const char *text, unsigned int maximum,
		     unsigned int *value)
{
	unsigned int result = 0;
	*value = 0;
	if (!text || !*text || (text[0] == '0' && text[1]))
		return false;
	for (; *text; text++)
	{
		unsigned int digit = (unsigned char)*text - '0';
		if (digit > 9 || digit > maximum ||
		    result > (maximum - digit) / 10)
			return false;
		result = result * 10 + digit;
	}
	*value = result;
	return true;
}

static int hex_digit(char digit)
{
	return digit >= '0' && digit <= '9'   ? digit - '0'
	       : digit >= 'a' && digit <= 'f' ? digit - 'a' + 10
					      : -1;
}

bool pinboard_hex(const char *text, char *bytes, size_t capacity)
{
	size_t length = strlen(text);
	bytes[0] = 0;
	if (!strcmp(text, "-"))
		return true;
	if (!length || length % 2 || length / 2 >= capacity)
		return false;
	for (size_t i = 0; i < length; i += 2)
	{
		int a = hex_digit(text[i]), b = hex_digit(text[i + 1]);
		if (a < 0 || b < 0 || (a == 0 && b == 0))
			return false;
		bytes[i / 2] = a * 16 + b;
	}
	bytes[length / 2] = 0;
	return true;
}

void pinboard_encode(const char *bytes, char *hex)
{
	static const char digits[] = "0123456789abcdef";
	for (; *bytes; bytes++)
	{
		unsigned char byte = (unsigned char)*bytes;
		*hex++ = digits[byte >> 4];
		*hex++ = digits[byte & 15];
	}
	*hex = 0;
}

static bool identity(const char *text)
{
	if (strlen(text) != 32)
		return false;
	for (; *text; text++)
		if (hex_digit(*text) < 0)
			return false;
	return true;
}

static bool printable_utf8(const char *text)
{
	const unsigned char *p = (const unsigned char *)text;
	while (*p)
	{
		unsigned int value = *p++, minimum = 0, count = 0;
		if (value >= 0xc2 && value <= 0xdf)
			count = 1, minimum = 0x80, value &= 31;
		else if (value >= 0xe0 && value <= 0xef)
			count = 2, minimum = 0x800, value &= 15;
		else if (value >= 0xf0 && value <= 0xf4)
			count = 3, minimum = 0x10000, value &= 7;
		else if (value >= 0x80)
			return false;
		for (unsigned int i = 0; i < count; i++)
		{
			if ((*p & 0xc0) != 0x80)
				return false;
			value = (value << 6) | (*p++ & 63);
		}
		if (value < minimum || value < 32 ||
		    (value >= 127 && value < 160) || value > 0x10ffff ||
		    (value >= 0xd800 && value <= 0xdfff) ||
		    (value >= 0x2028 && value <= 0x202f) ||
		    (value >= 0x2066 && value <= 0x2069))
			return false;
	}
	return true;
}

bool pinboard_layout_valid(pinboard_layout layout)
{
	return layout.width >= 64 && layout.width <= 512 &&
	       layout.height >= 64 && layout.height <= 512 &&
	       layout.icon >= 16 && layout.icon <= 128;
}

bool pinboard_pending_valid(const pinboard_model *model)
{
	if (!model->staging || model->poisoned ||
	    model->received != model->pending.count ||
	    !pinboard_layout_valid(model->pending.layout))
		return false;
	for (unsigned int i = 0; i < model->received; i++)
		if (!model->pending.entries[i].labelled)
			return false;
	return true;
}

void pinboard_publish(pinboard_model *model)
{
	model->current = model->pending;
	model->staging = model->poisoned = false;
	model->received = 0;
	memset(&model->pending, 0, sizeof(model->pending));
}

pinboard_request pinboard_receive(pinboard_model *model, const char *text,
				  size_t alias_length)
{
	pinboard_request result = {PINBOARD_BAD, "", 0, 0};
	char line[PINBOARD_LINE + 1], *words[10], *p;
	unsigned int count = 0, generation, old, number;
	pinboard_entry *entry = NULL;
	size_t length = strnlen(text, sizeof(line));

	if (length == sizeof(line) || alias_length > 64 ||
	    length + alias_length + strlen("SendToModule  ") > PINBOARD_LINE)
		goto bad;
	memcpy(line, text, length + 1);
	p = line;
	while (*p)
	{
		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p)
			break;
		if (count == 10)
			goto bad;
		words[count++] = p;
		while (*p && *p != ' ' && *p != '\t')
		{
			if ((unsigned char)*p < 33 || (unsigned char)*p > 126)
				goto bad;
			p++;
		}
		if (*p)
			*p++ = 0;
	}
	if (count < 2)
		goto bad;
	if (!strcmp(words[0], "Owner") && count == 2 && identity(words[1]))
	{
		if (model->owner[0] && strcmp(model->owner, words[1]))
			return result;
		strcpy(model->owner, words[1]);
		result.command = PINBOARD_OWNER;
		return result;
	}
	if (!model->owner[0] || strcmp(model->owner, words[1]))
		return result;
	if ((!strcmp(words[0], "Renew") || !strcmp(words[0], "Retire")) &&
	    count == 2)
	{
		result.command = !strcmp(words[0], "Renew") ? PINBOARD_RENEW
							    : PINBOARD_RETIRE;
		return result;
	}
	if ((!strcmp(words[0], "Status") && count == 3) ||
	    (!strcmp(words[0], "Cells") && count == 5))
	{
		if (!*words[2] || strlen(words[2]) > 64)
			goto bad;
		for (p = words[2]; *p; p++)
			if (!((*p >= 'a' && *p <= 'z') ||
			      (*p >= 'A' && *p <= 'Z') ||
			      (*p >= '0' && *p <= '9') || *p == '_' ||
			      *p == '-'))
				goto bad;
		strcpy(result.tag, words[2]);
		result.command = PINBOARD_STATUS;
		if (count == 5)
		{
			if (!pinboard_number(words[3], PINBOARD_MAX,
					     &result.offset) ||
			    !pinboard_number(words[4], 8, &result.limit) ||
			    !result.limit)
				goto bad;
			result.command = PINBOARD_CELLS;
		}
		return result;
	}
	if (!strcmp(words[0], "Begin") && count == 5)
	{
		if (!pinboard_number(words[2], INT_MAX, &old) ||
		    old != model->current.generation ||
		    !pinboard_number(words[3], INT_MAX, &generation) ||
		    generation <= old ||
		    !pinboard_number(words[4], PINBOARD_MAX, &number))
			goto bad;
		memset(&model->pending, 0, sizeof(model->pending));
		model->pending.generation = generation;
		model->pending.count = number;
		model->pending.layout = model->current.layout;
		model->received = 0;
		model->staging = true;
		model->poisoned = false;
		result.command = PINBOARD_BEGIN;
		return result;
	}
	if (count < 3 || !pinboard_number(words[2], INT_MAX, &generation) ||
	    !model->staging || generation != model->pending.generation)
		goto bad;
	if (!strcmp(words[0], "Abort") && count == 3)
	{
		model->staging = model->poisoned = false;
		model->received = 0;
		memset(&model->pending, 0, sizeof(model->pending));
		result.command = PINBOARD_ABORT;
		return result;
	}
	if (model->poisoned)
		goto bad;
	if (!strcmp(words[0], "Commit") && count == 3)
	{
		if (!pinboard_pending_valid(model))
			goto bad;
		result.command = PINBOARD_COMMIT;
		return result;
	}
	if (!strcmp(words[0], "Layout") && count == 6)
	{
		pinboard_layout layout;
		if (!pinboard_number(words[3], 512, &layout.width) ||
		    !pinboard_number(words[4], 512, &layout.height) ||
		    !pinboard_number(words[5], 128, &layout.icon) ||
		    !pinboard_layout_valid(layout))
			goto bad;
		model->pending.layout = layout;
		result.command = PINBOARD_STAGE;
		return result;
	}
	if (count < 4 || !identity(words[3]))
		goto bad;
	for (unsigned int i = 0; i < model->received; i++)
		if (!strcmp(words[3], model->pending.entries[i].id))
			entry = &model->pending.entries[i];
	if (!strcmp(words[0], "Entry") && count == 8)
	{
		pinboard_entry candidate = {0};
		if (entry || model->received == model->pending.count ||
		    !pinboard_hex(words[4], candidate.monitor,
				  sizeof(candidate.monitor)) ||
		    !pinboard_number(words[5], INT_MAX, &candidate.column) ||
		    !pinboard_number(words[6], INT_MAX, &candidate.row) ||
		    !pinboard_number(words[7], 1, &number))
			goto bad;
		strcpy(candidate.id, words[3]);
		candidate.missing = number;
		model->pending.entries[model->received++] = candidate;
	}
	else if (entry && !strcmp(words[0], "Label") && count == 5)
	{
		if (entry->labelled ||
		    !pinboard_hex(words[4], entry->label,
				  sizeof(entry->label)) ||
		    !printable_utf8(entry->label))
			goto bad;
		entry->labelled = true;
	}
	else if (entry && !strcmp(words[0], "Picture") && count == 5)
	{
		if (entry->pictured ||
		    !pinboard_hex(words[4], entry->picture,
				  sizeof(entry->picture)) ||
		    (entry->picture[0] && entry->picture[0] != '/'))
			goto bad;
		entry->pictured = true;
	}
	else
		goto bad;
	result.command = PINBOARD_STAGE;
	return result;
bad:
	if (model->staging)
		model->poisoned = true;
	result.command = PINBOARD_BAD;
	return result;
}

static bool drawable_valid(const rectangle *drawable)
{
	return drawable && drawable->width > 0 && drawable->height > 0 &&
	       drawable->width <= 32767 && drawable->height <= 32767 &&
	       drawable->x >= -32768 && drawable->y >= -32768 &&
	       (int64_t)drawable->x + drawable->width <= 32767 &&
	       (int64_t)drawable->y + drawable->height <= 32767;
}

static bool intersection(const module_monitor_area *area,
			 const rectangle *drawable, module_monitor_area *result)
{
	int64_t left = area->x > drawable->x ? area->x : drawable->x;
	int64_t top = area->y > drawable->y ? area->y : drawable->y;
	int64_t right = (int64_t)area->x + area->width;
	int64_t bottom = (int64_t)area->y + area->height;
	if (right > (int64_t)drawable->x + drawable->width)
		right = (int64_t)drawable->x + drawable->width;
	if (bottom > (int64_t)drawable->y + drawable->height)
		bottom = (int64_t)drawable->y + drawable->height;
	*result = (module_monitor_area){0};
	if (left >= right || top >= bottom)
		return false;
	*result = (module_monitor_area){left, top, right - left, bottom - top};
	return true;
}

static bool grid_prepare(const pinboard_layout *layout,
			 const module_ui_scale_snapshot *snapshot,
			 const module_workarea_state *workareas,
			 unsigned int monitor, module_monitor_area *area,
			 pinboard_cell *cell)
{
	const module_ui_scale_row *source = &snapshot->rows[monitor];
	*cell = (pinboard_cell){0};
	if (!module_workarea_bounds(workareas, snapshot, source, area) ||
	    monitor_scale_apply(layout->width,
				(monitor_scale_factor){source->quarters},
				&cell->width) != MONITOR_SCALE_CONVERSION_OK ||
	    monitor_scale_apply(layout->height,
				(monitor_scale_factor){source->quarters},
				&cell->height) != MONITOR_SCALE_CONVERSION_OK ||
	    cell->width < 1 || cell->height < 1)
		return false;
	strcpy(cell->monitor, source->name);
	cell->quarters = source->quarters;
	return true;
}

static bool cell_at(const pinboard_layout *layout,
		    const module_ui_scale_snapshot *snapshot,
		    const module_workarea_state *workareas,
		    const rectangle *drawable, unsigned int monitor,
		    unsigned int column, unsigned int row, pinboard_cell *cell)
{
	module_monitor_area area;
	int64_t x, y;
	if (!grid_prepare(layout, snapshot, workareas, monitor, &area, cell))
		return false;
	cell->column = column;
	cell->row = row;
	x = (int64_t)area.x + (int64_t)column * cell->width;
	y = (int64_t)area.y + (int64_t)row * cell->height;
	if (x < -32768 || y < -32768 || x + cell->width > 32767 ||
	    y + cell->height > 32767)
		return false;
	cell->x = x;
	cell->y = y;
	if (x + cell->width > (int64_t)area.x + area.width ||
	    y + cell->height > (int64_t)area.y + area.height ||
	    x < drawable->x || y < drawable->y ||
	    x + cell->width > (int64_t)drawable->x + drawable->width ||
	    y + cell->height > (int64_t)drawable->y + drawable->height)
		return false;
	cell->visible = true;
	return true;
}

static bool overlaps(const pinboard_cell *a, const pinboard_cell *b)
{
	return a->visible && b->visible && a->x < b->x + b->width &&
	       b->x < a->x + a->width && a->y < b->y + b->height &&
	       b->y < a->y + a->height;
}

static bool vacant(const pinboard_projection *projection, unsigned int count,
		   const pinboard_cell *cell)
{
	if (overlaps(&projection->manage, cell))
		return false;
	for (unsigned int i = 0; i < count; i++)
		if (overlaps(&projection->cells[i], cell))
			return false;
	return true;
}

static bool management_cell(const pinboard_layout *layout,
			    const module_ui_scale_snapshot *snapshot,
			    const module_workarea_state *workareas,
			    const rectangle *drawable, unsigned int primary,
			    pinboard_cell *cell)
{
	module_monitor_area area, available;
	/* Prefer primary intersection, then the first other intersecting row.
	 */
	for (unsigned int m = 0; m <= snapshot->count; m++)
	{
		unsigned int index = m == 0 ? primary : m - 1;
		if (m && index == primary)
			continue;
		if (!grid_prepare(layout, snapshot, workareas, index, &area,
				  cell))
			continue;
		if (!intersection(&area, drawable, &available))
			continue;
		cell->x = available.x;
		cell->y = available.y;
		if (cell->width > available.width)
			cell->width = available.width;
		if (cell->height > available.height)
			cell->height = available.height;
		cell->visible = true;
		return true;
	}
	/* No monitor intersects the real root drawable: retain one bounded
	 * recovery control, without pretending off-root pins are visible. */
	if (!grid_prepare(layout, snapshot, workareas, primary, &area, cell))
		return false;
	cell->x = drawable->x;
	cell->y = drawable->y;
	if (cell->width > drawable->width)
		cell->width = drawable->width;
	if (cell->height > drawable->height)
		cell->height = drawable->height;
	cell->visible = true;
	return true;
}

bool pinboard_project(const pinboard_scene *scene,
		      const module_ui_scale_snapshot *snapshot,
		      const module_workarea_state *workareas,
		      const rectangle *drawable,
		      pinboard_projection *projection)
{
	unsigned int primary = 0;
	*projection = (pinboard_projection){0};
	if (scene->count > PINBOARD_MAX ||
	    !pinboard_layout_valid(scene->layout) ||
	    !drawable_valid(drawable) || !module_ui_scale_validate(snapshot))
		return false;
	projection->epoch = snapshot->epoch;
	for (unsigned int i = 0; i < snapshot->count; i++)
		if (snapshot->rows[i].primary)
			primary = i;
	if (!management_cell(&scene->layout, snapshot, workareas, drawable,
			     primary, &projection->manage))
		return false;
	for (unsigned int i = 0; i < scene->count; i++)
	{
		const pinboard_entry *entry = &scene->entries[i];
		unsigned int preferred = primary;
		pinboard_cell candidate;
		for (unsigned int j = 0; j < snapshot->count; j++)
			if (!strcmp(entry->monitor, snapshot->rows[j].name))
				preferred = j;
		if (cell_at(&scene->layout, snapshot, workareas, drawable,
			    preferred, entry->column, entry->row, &candidate) &&
		    vacant(projection, i, &candidate))
		{
			projection->cells[i] = candidate;
			continue;
		}
		projection->cells[i] = candidate;
		projection->cells[i].visible = false;
		for (unsigned int m = 0; m < snapshot->count; m++)
		{
			unsigned int j = (preferred + m) % snapshot->count;
			module_monitor_area area, available;
			pinboard_cell grid;
			if (!grid_prepare(&scene->layout, snapshot, workareas,
					  j, &area, &grid) ||
			    !intersection(&area, drawable, &available) ||
			    available.width < grid.width ||
			    available.height < grid.height)
				continue;
			/* Keep the original work-area grid phase. Compute the
			 * first and last fully contained cells with nonnegative
			 * int64 math. */
			int64_t first_column =
			    ((int64_t)available.x - area.x + grid.width - 1) /
			    grid.width;
			int64_t first_row =
			    ((int64_t)available.y - area.y + grid.height - 1) /
			    grid.height;
			int64_t last_column =
			    ((int64_t)available.x + available.width - area.x -
			     grid.width) /
			    grid.width;
			int64_t last_row =
			    ((int64_t)available.y + available.height - area.y -
			     grid.height) /
			    grid.height;
			if (first_column > last_column || first_row > last_row)
				continue;
			unsigned int rows = last_row - first_row + 1;
			for (unsigned int slot = 0; slot <= PINBOARD_MAX;
			     slot++)
			{
				int64_t column = first_column + slot / rows;
				if (column > last_column)
					break;
				if (cell_at(&scene->layout, snapshot, workareas,
					    drawable, j, column,
					    first_row + slot % rows,
					    &candidate) &&
				    vacant(projection, i, &candidate))
				{
					projection->cells[i] = candidate;
					break;
				}
			}
			if (projection->cells[i].visible)
				break;
		}
	}
	return true;
}

bool pinboard_move_cell(const pinboard_layout *layout,
			const module_ui_scale_snapshot *snapshot,
			const module_workarea_state *workareas,
			const rectangle *drawable, int x, int y,
			pinboard_cell *cell)
{
	*cell = (pinboard_cell){0};
	if (!pinboard_layout_valid(*layout) || !drawable_valid(drawable) ||
	    !module_ui_scale_validate(snapshot) || x < drawable->x ||
	    y < drawable->y ||
	    (int64_t)x >= (int64_t)drawable->x + drawable->width ||
	    (int64_t)y >= (int64_t)drawable->y + drawable->height)
		return false;
	for (unsigned int i = 0; i < snapshot->count; i++)
	{
		module_monitor_area area;
		pinboard_cell grid;
		if (!grid_prepare(layout, snapshot, workareas, i, &area,
				  &grid) ||
		    x < area.x || y < area.y ||
		    (int64_t)x >= (int64_t)area.x + area.width ||
		    (int64_t)y >= (int64_t)area.y + area.height)
			continue;
		if (cell_at(layout, snapshot, workareas, drawable, i,
			    ((int64_t)x - area.x) / grid.width,
			    ((int64_t)y - area.y) / grid.height, cell))
			return true;
	}
	return false;
}

bool pinboard_picture_clip(const pinboard_cell *cell, int picture_width,
			   int picture_height, pinboard_blit *copy)
{
	int64_t x, y, left, top, right, bottom;
	*copy = (pinboard_blit){0};
	if (!cell || cell->width < 1 || cell->height < 1 || picture_width < 1 ||
	    picture_width > 32767 || picture_height < 1 ||
	    picture_height > 32767)
		return false;
	x = (int64_t)cell->x + ((int64_t)cell->width - picture_width) / 2;
	y = (int64_t)cell->y + 4;
	left = x > cell->x ? x : cell->x;
	top = y > cell->y ? y : cell->y;
	right = x + picture_width;
	bottom = y + picture_height;
	if (right > (int64_t)cell->x + cell->width)
		right = (int64_t)cell->x + cell->width;
	if (bottom > (int64_t)cell->y + cell->height)
		bottom = (int64_t)cell->y + cell->height;
	if (left >= right || top >= bottom || left < INT_MIN || top < INT_MIN ||
	    left > INT_MAX || top > INT_MAX)
		return false;
	*copy = (pinboard_blit){left - x, top - y,	left,
				top,	  right - left, bottom - top};
	return true;
}

bool pinboard_workspace_publication(pinboard_workspace *state,
				    module_ui_scale_status status,
				    bool request_pending, bool active_view)
{
	if (status != MODULE_UI_SCALE_PUBLISHED)
		return false;
	memset(state, 0, sizeof(*state));
	return !request_pending || active_view;
}

bool pinboard_workspace_changed(pinboard_workspace *state, bool page,
				const unsigned long *body, size_t words)
{
	unsigned long output;
	pinboard_workspace_row *row = NULL, *empty = NULL;
	bool changed;
	if (!body || words < (page ? 8U : 2U))
		return false;
	output = body[page ? 7 : 1];
	for (unsigned int i = 0; i < MODULE_UI_SCALE_MAX; i++)
	{
		if (state->rows[i].used && state->rows[i].output == output)
			row = &state->rows[i];
		if (!state->rows[i].used && !empty)
			empty = &state->rows[i];
	}
	if (!row)
	{
		if (!empty)
			return false;
		row = empty;
		row->used = true;
		row->output = output;
	}
	if (page)
	{
		changed =
		    row->has_page && memcmp(row->page, body, sizeof(row->page));
		if (row->has_desk && row->desk != body[2])
			changed = true;
		memcpy(row->page, body, sizeof(row->page));
		row->has_page = row->has_desk = true;
		row->desk = body[2];
	}
	else
	{
		changed = row->has_desk && row->desk != body[0];
		row->desk = body[0];
		row->has_desk = true;
		if (row->has_page)
			row->page[2] = body[0];
	}
	return changed;
}

pinboard_config pinboard_config_parse(char *text, const char *alias,
				      char **value)
{
	static const struct
	{
		const char *name;
		pinboard_config config;
	} options[] = {{"Font", PINBOARD_CONFIG_FONT},
		       {"Colorset", PINBOARD_CONFIG_COLORSET},
		       {"CellWidth", PINBOARD_CONFIG_WIDTH},
		       {"CellHeight", PINBOARD_CONFIG_HEIGHT},
		       {"IconSize", PINBOARD_CONFIG_ICON}};
	size_t length = strlen(text), prefix = strlen(alias);
	*value = NULL;
	if (length > PINBOARD_LINE || *text != '*' ||
	    strncasecmp(text + 1, alias, prefix))
		return PINBOARD_CONFIG_NONE;
	char *option = text + 1 + prefix;
	if (*option == ':')
		option++;
	while (*option == ' ' || *option == '\t')
		option++;
	for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); i++)
	{
		size_t size = strlen(options[i].name);
		if (strncasecmp(option, options[i].name, size) ||
		    (option[size] != ' ' && option[size] != '\t'))
			continue;
		char *start = option + size;
		while (*start == ' ' || *start == '\t')
			start++;
		char *end = text + length;
		while (end > start && (end[-1] == ' ' || end[-1] == '\t' ||
				       end[-1] == '\n' || end[-1] == '\r'))
			*--end = 0;
		if (!*start)
			return PINBOARD_CONFIG_NONE;
		*value = start;
		return options[i].config;
	}
	return PINBOARD_CONFIG_NONE;
}
