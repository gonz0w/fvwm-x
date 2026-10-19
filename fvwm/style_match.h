#ifndef FVWM_STYLE_MATCH_H
#define FVWM_STYLE_MATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include "style_id.h"
#include "style_match_types.h"

#define STYLE_LOOKUP_BUCKETS 256
#define STYLE_LOOKUP_STREAMS 9
#define STYLE_LOOKUP_WINDOW (1U << (STYLE_MATCH_FIELDS + 1))
#define STYLE_LOOKUP_ALL ((STYLE_LOOKUP_WINDOW << 1) - 1)

/* Borrowed only for one synchronous lookup/mark operation. Hashes, lengths
 * and compatibility representatives are lazy, shared across lists/domains,
 * and reset with the identity. None is a retained window-match result. */
typedef struct
{
	Window window;
	const char *value[STYLE_MATCH_FIELDS];
	size_t length[STYLE_MATCH_FIELDS];
	unsigned compatibility_fields;
	unsigned hash_fields;
	uint64_t hash[STYLE_MATCH_FIELDS];
	unsigned window_bucket;
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
unsigned style_match_lookup_subject(style_match_subject *subject,
	unsigned domains, unsigned buckets[STYLE_LOOKUP_STREAMS]);
bool style_match_id(const struct style_id_t *id,
	const style_match_patterns *patterns, style_match_subject *subject);

#endif
