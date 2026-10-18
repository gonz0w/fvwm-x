#ifndef FVWM_STYLE_MATCH_TYPES_H
#define FVWM_STYLE_MATCH_TYPES_H

#include <stddef.h>
#include <stdint.h>

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
	unsigned lookup_domain;
	size_t prefix[STYLE_MATCH_FIELDS];
	size_t minimum[STYLE_MATCH_FIELDS];
	size_t suffix[STYLE_MATCH_FIELDS];
	size_t suffix_length[STYLE_MATCH_FIELDS];
	unsigned literal;
	unsigned universal;
	unsigned unescaped;
	unsigned single_star;
} style_match_patterns;

#endif
