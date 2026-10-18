#ifndef FVWM_STYLE_MATCH_H
#define FVWM_STYLE_MATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <X11/Xlib.h>

struct style_id_t;

#define STYLE_LOOKUP_BUCKETS 256
#define STYLE_LOOKUP_STREAMS 9

enum style_match_field
{
	STYLE_MATCH_RESOURCE,
	STYLE_MATCH_CLASS,
	STYLE_MATCH_NAME,
	STYLE_MATCH_ICON,
	STYLE_MATCH_FIELDS
};

/* Derived from the owning immutable selector ID; no pointers. */
typedef struct
{
	uint64_t fingerprint;
	unsigned lookup_bucket;
	size_t prefix[STYLE_MATCH_FIELDS];
	size_t minimum[STYLE_MATCH_FIELDS];
	size_t suffix[STYLE_MATCH_FIELDS];
	size_t suffix_length[STYLE_MATCH_FIELDS];
	unsigned literal;
	unsigned universal;
	unsigned unescaped;
	unsigned single_star;
} style_match_patterns;

/* Borrowed only for one synchronous lookup/mark operation. Lengths are
 * derived lazily, at most once per field, and reset with the identity. */
typedef struct
{
	Window window;
	const char *value[STYLE_MATCH_FIELDS];
	size_t length[STYLE_MATCH_FIELDS];
	unsigned compatibility_fields;
	unsigned length_fields;
} style_match_subject;

void style_match_prepare(style_match_patterns *patterns,
	const struct style_id_t *id);
void style_match_subject_init(style_match_subject *subject, Window window,
	const char *resource, const char *class, const char *name, const char *icon);
bool style_match_ids_may_overlap(const struct style_id_t *a,
	const style_match_patterns *ap, const struct style_id_t *b,
	const style_match_patterns *bp);
bool style_match_lookup_enabled(size_t count, size_t indexed);
unsigned style_match_lookup_subject(const style_match_subject *subject,
	unsigned buckets[STYLE_LOOKUP_STREAMS]);
bool style_match_id(const struct style_id_t *id,
	const style_match_patterns *patterns, style_match_subject *subject);

#endif
