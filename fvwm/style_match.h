#ifndef FVWM_STYLE_MATCH_H
#define FVWM_STYLE_MATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <X11/Xlib.h>

struct style_id_t;

enum style_match_field
{
	STYLE_MATCH_RESOURCE,
	STYLE_MATCH_CLASS,
	STYLE_MATCH_NAME,
	STYLE_MATCH_ICON,
	STYLE_MATCH_FIELDS
};

/* Derived only from the owning immutable selector strings; no pointers. */
typedef struct
{
	size_t prefix[STYLE_MATCH_FIELDS];
	unsigned literal;
	unsigned universal;
} style_match_patterns;

/* Borrowed only for one synchronous lookup/mark operation. */
typedef struct
{
	Window window;
	const char *value[STYLE_MATCH_FIELDS];
	unsigned compatibility_fields;
} style_match_subject;

void style_match_prepare(style_match_patterns *patterns,
	const struct style_id_t *id);
void style_match_subject_init(style_match_subject *subject, Window window,
	const char *resource, const char *class, const char *name, const char *icon);
bool style_match_id(const struct style_id_t *id,
	const style_match_patterns *patterns, const style_match_subject *subject);

#endif
