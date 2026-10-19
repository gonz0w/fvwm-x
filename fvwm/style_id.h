#ifndef FVWM_STYLE_ID_H
#define FVWM_STYLE_ID_H

#include <X11/X.h>

/* Selector identity shared by the pure matcher and the style owner. */
typedef struct style_id_flags
{
	unsigned has_name:1;
	unsigned has_window_id:1;
	unsigned has_class:1;
	unsigned has_resource:1;
	unsigned has_icon:1;
	unsigned is_compatibility_mode:1;
} style_id_flags;

typedef struct style_id_t
{
	XID window_id;
	style_id_flags flags;
	char *name;
	char *class;
	char *resource;
	char *icon;
} style_id_t;

#endif
