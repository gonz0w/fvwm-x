#ifndef FVWM_FORM_LIST_H
#define FVWM_FORM_LIST_H
#include <stdbool.h>
#include <stddef.h>
#define FORM_LIST_MAX 4096
#define FORM_LIST_SELECTED 256
#define FORM_LIST_BYTES (FORM_LIST_MAX / 8)
enum form_list_icon
{
  FORM_ICON_NONE, FORM_ICON_FILE, FORM_ICON_DIRECTORY,
  FORM_ICON_LINK, FORM_ICON_OTHER
};
bool form_list_parse_icon(const char *text, enum form_list_icon *icon);
/* Indices belong to one immutable configuration, never to filenames. */
typedef struct {
	int count, rows, columns, top, focus, anchor, selected;
	bool refused;
	unsigned char bits[FORM_LIST_BYTES];
} form_list_state;
bool form_list_number(const char *text, int maximum, int *value);
bool form_list_add(form_list_state *state, bool selected);
int form_list_columns(const form_list_state *state);
int form_list_lines(const form_list_state *state);
bool form_list_extend(form_list_state *state, int index);
bool form_list_toggle(form_list_state *state, int index);
void form_list_focus(form_list_state *state, int index);
void form_list_scroll(form_list_state *state, int amount);
size_t form_list_packet(const form_list_state *state,
	unsigned char packet[FORM_LIST_BYTES + 2]);
#endif
