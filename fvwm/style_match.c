/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <string.h>
#include "fvwm.h"
#include "style_match.h"
#include "style_id_filter.h"
#include "libs/wild.h"

/* Zero is the ordered fallback chain. All other chains are conservative
 * hash candidates: equality and every remaining constraint still use the
 * matcher. Field tags keep explicit predicates separate from the legacy OR. */
static unsigned lookup_bucket(uint64_t hash)
{
	return 1 + hash % (STYLE_LOOKUP_BUCKETS - 1);
}

static uint64_t string_hash(const char *value, size_t *length)
{
	uint64_t hash = UINT64_C(14695981039346656037);

	for (*length = 0; *value; value++, (*length)++)
		hash = (hash ^ (unsigned char)*value) * UINT64_C(1099511628211);
	return hash;
}

static unsigned string_bucket(unsigned field, const char *value)
{
	size_t length;

	return lookup_bucket(string_hash(value, &length) ^ field);
}

static unsigned window_bucket(Window window)
{
	uint64_t hash = (uint64_t)window;

	hash ^= hash >> 30;
	hash *= UINT64_C(0xbf58476d1ce4e5b9);
	hash ^= hash >> 27;
	hash *= UINT64_C(0x94d049bb133111eb);
	return lookup_bucket(hash ^ (hash >> 31));
}

bool style_match_lookup_enabled(size_t count, size_t indexed)
{
	/* Small and mostly general-wildcard lists retain a linear walk. */
	return indexed >= 32 && indexed >= count - indexed;
}

static void prepare_compatibility_fields(style_match_subject *subject);

static unsigned subject_bucket(style_match_subject *subject, unsigned field,
	unsigned domain)
{
	unsigned bit = 1U << field;

	if (!(subject->hash_fields & bit))
	{
		/* Hashing already reaches the terminator. Publish its length
		 * for later wildcard checks in this same borrowed identity. */
		subject->hash[field] = string_hash(subject->value[field],
			&subject->length[field]);
		subject->length_fields |= bit;
		subject->hash_fields |= bit;
	}
	return lookup_bucket(subject->hash[field] ^ domain);
}

unsigned style_match_lookup_subject(style_match_subject *subject,
	unsigned domains, unsigned buckets[STYLE_LOOKUP_STREAMS])
{
	unsigned count = 0;
	unsigned possible[STYLE_LOOKUP_STREAMS];

	possible[count++] = 0;
	if (domains & STYLE_LOOKUP_WINDOW)
	{
		if (!subject->window_bucket)
			subject->window_bucket = window_bucket(subject->window);
		possible[count++] = subject->window_bucket;
	}
	for (unsigned i = 0; i < STYLE_MATCH_FIELDS; i++)
	{
		if ((domains & (1U << i)) && subject->value[i])
			possible[count++] = subject_bucket(subject, i, i);
	}
	if (domains & (1U << STYLE_MATCH_FIELDS))
	{
		prepare_compatibility_fields(subject);
		for (unsigned i = 0; i <= STYLE_MATCH_NAME; i++)
		{
			if ((subject->compatibility_fields & (1U << i)) &&
			    subject->value[i])
				possible[count++] = subject_bucket(subject, i,
					STYLE_MATCH_FIELDS);
		}
	}
	/* Hash collisions and equal subject fields must never visit a chain
	 * twice: duplicate matching merges can duplicate ownership. */
	unsigned used = 0;

	for (unsigned i = 0; i < count; i++)
	{
		unsigned j;

		for (j = 0; j < used && buckets[j] != possible[i]; j++);
		if (j == used)
			buckets[used++] = possible[i];
	}
	return used;
}

static void prepare_escaped_pattern(style_match_patterns *patterns,
	const char *value, unsigned field)
{
	size_t stars = 0, width = 0, before = 0;
	unsigned bit = 1U << field;

	for (size_t i = 0; value[i]; i++)
	{
		if (value[i] == '\\')
		{
			/* A trailing escape has an impossible literal slot.
			 * Matching never reads past its terminator. */
			if (value[i + 1])
				i++;
			width++;
		}
		else if (value[i] == '*')
		{
			stars++;
			before = width;
			patterns->suffix[field] = i + 1;
		}
		else
			width++;
	}
	patterns->escaped |= bit;
	patterns->minimum[field] = width;
	patterns->suffix_length[field] = width - before;
	if (stars == 0)
		patterns->escaped_fixed_width |= bit;
	if (stars == 1)
		patterns->escaped_single_star |= bit;
}

void style_match_prepare(style_match_patterns *patterns, const style_id_t *id)
{
	const char *values[STYLE_MATCH_FIELDS] = {
		id->resource, id->class, id->name, id->icon
	};
	bool active[STYLE_MATCH_FIELDS] = {
		id->flags.has_resource, id->flags.has_class,
		id->flags.has_name || id->flags.is_compatibility_mode,
		id->flags.has_icon
	};

	memset(patterns, 0, sizeof(*patterns));
	patterns->fingerprint = style_id_hash(id);
	for (unsigned i = 0; i < STYLE_MATCH_FIELDS; i++)
	{
		const char *value = values[i];
		size_t length = 0, stars = 0, questions = 0;
		unsigned bit = 1U << i;

		if (!active[i])
			continue;
		/* Preserve the wildcard owner's NULL and exact-"*" semantics.
		 * Escaped fields retain separate decoded metadata. */
		if (!value || (value[0] == '*' && value[1] == '\0'))
		{
			patterns->universal |= bit;
			continue;
		}
		patterns->prefix[i] = strcspn(value, "*?\\");
		if (value[patterns->prefix[i]] == '\0')
		{
			patterns->literal |= bit;
			continue;
		}
		/* Wildcards consume bytes. Escapes quote exactly the next
		 * byte; a trailing escape remains a non-match. */
		if (strchr(value, '\\'))
		{
			prepare_escaped_pattern(patterns, value, i);
			continue;
		}
		patterns->unescaped |= bit;

		for (; value[length]; length++)
		{
			if (value[length] == '*')
				stars++;
			else
				patterns->minimum[i]++;
			if (value[length] == '?')
				questions++;
			if (value[length] == '*' || value[length] == '?')
				patterns->suffix[i] = length + 1;
		}
		patterns->suffix_length[i] = length - patterns->suffix[i];
		if (stars == 1 && questions == 0)
			patterns->single_star |= bit;
		if (stars == 0)
			patterns->fixed_width |= bit;
		if (stars == 1 && questions != 0)
			patterns->single_star_questions |= bit;
	}
	/* Pick one necessary predicate, never a sufficient match. Numeric
	 * identities win; explicit literal conjunctions precede the legacy OR.
	 * NULL, universal, wildcard and escaped selectors remain on fallback
	 * unless another independent necessary predicate can index the rule. */
	if (id->flags.has_window_id)
	{
		patterns->lookup_bucket = window_bucket(id->window_id);
		patterns->lookup_domain = STYLE_LOOKUP_WINDOW;
	}
	else
	{
		for (unsigned i = 0; i < STYLE_MATCH_FIELDS; i++)
		{
			if (active[i] && (patterns->literal & (1U << i)) &&
			    (i != STYLE_MATCH_NAME || id->flags.has_name))
			{
				patterns->lookup_bucket =
					string_bucket(i, values[i]);
				patterns->lookup_domain = 1U << i;
				break;
			}
		}
		if (!patterns->lookup_bucket &&
		    id->flags.is_compatibility_mode &&
		    (patterns->literal & (1U << STYLE_MATCH_NAME)))
		{
			patterns->lookup_bucket =
				string_bucket(STYLE_MATCH_FIELDS, id->name);
			patterns->lookup_domain = 1U << STYLE_MATCH_FIELDS;
		}
	}

}

void style_match_subject_init(style_match_subject *subject, Window window,
	const char *resource, const char *class, const char *name, const char *icon)
{
	subject->window = window;
	subject->value[STYLE_MATCH_RESOURCE] = resource;
	subject->value[STYLE_MATCH_CLASS] = class;
	subject->value[STYLE_MATCH_NAME] = name;
	subject->value[STYLE_MATCH_ICON] = icon;
	subject->length_fields = 0;
	subject->compatibility_fields = 0;
	subject->hash_fields = 0;
	subject->window_bucket = 0;
}

static void prepare_compatibility_fields(style_match_subject *subject)
{
	/* Bit zero is always a representative, including NULL, so zero also
	 * means unprepared. Explicit-only and numeric rejects do no equality
	 * work; compatibility rules share one ordered representative set. */
	if (subject->compatibility_fields)
		return;
	/* Equality is computed once per lookup, not once per rule. Keep the
	 * first representative, including NULL, in original evaluation order. */
	for (unsigned i = 0; i <= STYLE_MATCH_NAME; i++)
	{
		unsigned j;

		for (j = 0; j < i; j++)
		{
			if (subject->value[i] == subject->value[j] ||
			    (subject->value[i] && subject->value[j] &&
			     strcmp(subject->value[i], subject->value[j]) == 0))
				break;
		}
		if (j == i)
			subject->compatibility_fields |= 1U << i;
	}
}

/* The caller proves that this unescaped fixed-width span fits in value. */
static bool match_fixed_span(const char *pattern, const char *value,
	size_t length)
{
	for (size_t i = 0; i < length; i++)
	{
		if (pattern[i] != '?' && pattern[i] != value[i])
			return false;
	}
	return true;
}

static bool match_unescaped_cursors(const char *pattern, const char *value)
{
	const char *retry_pattern = NULL, *retry_value = NULL;

	while (*value)
	{
		if (*pattern == '*')
		{
			pattern++;
			if (!*pattern)
				return true;
			retry_pattern = pattern;
			retry_value = value;
		}
		else if (*pattern == '?' || *pattern == *value)
		{
			pattern++;
			value++;
		}
		else if (retry_pattern)
		{
			/* Retry the latest star with one more byte. Its cursor
			 * never passes the current non-NUL value, so advancing
			 * can reach the terminator but cannot pass it. */
			pattern = retry_pattern;
			value = ++retry_value;
		}
		else
			return false;
	}
	/* Preserve legacy exhaustion instead of skipping all remaining stars:
	 * "**" fails empty text, as does "a**" after consuming exactly "a". */
	return !*pattern || (*pattern == '*' && !pattern[1]);
}

/* Preparation proves that width decoded bytes fit in the value. */
static bool match_escaped_span(const char *pattern, const char *value,
	size_t width)
{
	for (size_t i = 0; i < width; i++, pattern++, value++)
	{
		bool question = *pattern == '?';

		if (*pattern == '\\')
		{
			pattern++;
			question = false;
		}
		if (!*pattern || (!question && *pattern != *value))
			return false;
	}
	return true;
}

static bool match_escaped_cursors(const char *pattern, const char *value)
{
	const char *retry_pattern = NULL, *retry_value = NULL;

	while (*value)
	{
		if (*pattern == '*')
		{
			pattern++;
			if (!*pattern)
				return true;
			retry_pattern = pattern;
			retry_value = value;
		}
		else
		{
			const char *literal = pattern;
			bool question = *literal == '?';

			if (*literal == '\\')
			{
				literal++;
				question = false;
			}
			/* A trailing escape reaches only the terminator.
			 * Quoted wildcards are ordinary literal bytes here. */
			if (*literal && (question || *literal == *value))
			{
				pattern = literal + 1;
				value++;
			}
			else if (retry_pattern)
			{
				pattern = retry_pattern;
				value = ++retry_value;
			}
			else
				return false;
		}
	}
	/* The original owner accepts only a single raw trailing star on
	 * exhaustion. A quoted star or multiple remaining stars still fail. */
	return !*pattern || (*pattern == '*' && !pattern[1]);
}

static bool match_pattern(const char *pattern,
	const style_match_patterns *patterns, unsigned field,
	style_match_subject *subject, unsigned subject_field)
{
	unsigned bit = 1U << field;
	const char *value = subject->value[subject_field];

	if (patterns->universal & bit)
		return true;
	if (!value)
		return false;
	/* An earlier hash/match may already know this byte count. Reject
	 * impossible widths before revisiting literal or prefix bytes; never
	 * add a length scan for a rule that previously needed none. */
	if (subject->length_fields & (1U << subject_field))
	{
		size_t length = subject->length[subject_field];

		if (patterns->literal & bit)
		{
			if (length != patterns->prefix[field])
				return false;
		}
		else if ((patterns->unescaped | patterns->escaped) & bit)
		{
			if (length < patterns->minimum[field] ||
			    (((patterns->fixed_width |
			       patterns->escaped_fixed_width) & bit) &&
			     length != patterns->minimum[field]))
				return false;
		}
	}
	if (patterns->literal & bit)
		return strcmp(pattern, value) == 0;
	/* This span contains no wildcard or escape. A mismatch cannot be
	 * repaired by the suffix; successful prefixes need the remaining checks. */
	if (strncmp(pattern, value, patterns->prefix[field]) != 0)
		return false;
	if (patterns->unescaped & bit)
	{
		unsigned subject_bit = 1U << subject_field;
		size_t prefix = patterns->prefix[field];
		size_t suffix_length = patterns->suffix_length[field];
		size_t length;

		/* A trailing single star needs no full-value scan: the prefix
		 * check already proves its entire minimum length. All-star
		 * general patterns have no length/suffix test to perform. */
		if ((patterns->single_star & bit) && suffix_length == 0)
			return true;
		if (patterns->minimum[field] == 0 && suffix_length == 0)
			return match_unescaped_cursors(pattern, value);
		if (!(subject->length_fields & subject_bit))
		{
			subject->length[subject_field] = strlen(value);
			subject->length_fields |= subject_bit;
		}
		length = subject->length[subject_field];
		if (length < patterns->minimum[field] ||
		    ((patterns->fixed_width & bit) &&
		     length != patterns->minimum[field]))
			return false;

		/* minimum includes every suffix byte, so this subtraction cannot
		 * underflow. Only the final literal span is anchored at the end. */
		if (suffix_length && memcmp(pattern + patterns->suffix[field],
		    value + length - suffix_length, suffix_length) != 0)
			return false;
		/* Exactly one unescaped star and no '?' leaves only the two
		 * verified spans. minimum prevents prefix/suffix overlap. */
		if (patterns->single_star & bit)
			return true;
		/* No star leaves an exact byte count. Prefix/suffix literals
		 * are already verified; compare only the intervening span. */
		if (patterns->fixed_width & bit)
			return match_fixed_span(
				pattern + prefix, value + prefix,
				length - prefix - suffix_length);
		if (patterns->single_star_questions & bit)
		{
			const char *star = strchr(pattern, '*');
			size_t before = star - pattern;
			size_t after = patterns->minimum[field] - before;

			/* The minimum keeps these two fixed-width spans apart.
			 * Their literal ends were checked above. */
			if (!match_fixed_span(pattern + prefix,
			    value + prefix, before - prefix))
				return false;
			return match_fixed_span(star + 1,
				value + length - after, after - suffix_length);
		}
		/* No retry point precedes the verified initial literal span. */
		return match_unescaped_cursors(
			pattern + prefix, value + prefix);
	}
	if (patterns->escaped & bit)
	{
		unsigned subject_bit = 1U << subject_field;

		if (!(subject->length_fields & subject_bit))
		{
			subject->length[subject_field] = strlen(value);
			subject->length_fields |= subject_bit;
		}
		size_t length = subject->length[subject_field];
		/* Initial literal bytes have equal raw and decoded widths. */
		size_t prefix = patterns->prefix[field];

		if (length < patterns->minimum[field] ||
		    ((patterns->escaped_fixed_width & bit) &&
		     length != patterns->minimum[field]))
			return false;
		if (patterns->escaped_fixed_width & bit)
			return match_escaped_span(pattern + prefix,
				value + prefix,
				patterns->minimum[field] - prefix);
		if (!(patterns->escaped_single_star & bit))
			return match_escaped_cursors(
				pattern + prefix, value + prefix);
		size_t after = patterns->suffix_length[field];
		size_t before = patterns->minimum[field] - after;

		/* One unescaped star separates two decoded fixed-width spans.
		 * The minimum keeps them apart, including quoted wildcards. */
		return match_escaped_span(pattern + prefix,
			value + prefix, before - prefix) &&
			match_escaped_span(pattern + patterns->suffix[field],
				value + length - after, after);
	}
	return matchWildcards(pattern, value) != 0;
}

/* Only explicit conjunctions are reordered. Within each group preserve
 * resource/class/name/icon order; the compatibility OR stays separate. */
static bool match_explicit_fields(const style_id_t *id,
	const style_match_patterns *patterns, style_match_subject *subject,
	unsigned fields)
{
	if ((fields & (1U << STYLE_MATCH_RESOURCE)) &&
	    !match_pattern(id->resource, patterns, STYLE_MATCH_RESOURCE,
		subject, STYLE_MATCH_RESOURCE))
		return false;
	if ((fields & (1U << STYLE_MATCH_CLASS)) &&
	    !match_pattern(id->class, patterns, STYLE_MATCH_CLASS,
		subject, STYLE_MATCH_CLASS))
		return false;
	if ((fields & (1U << STYLE_MATCH_NAME)) &&
	    !match_pattern(id->name, patterns, STYLE_MATCH_NAME,
		subject, STYLE_MATCH_NAME))
		return false;
	if ((fields & (1U << STYLE_MATCH_ICON)) &&
	    !match_pattern(id->icon, patterns, STYLE_MATCH_ICON,
		subject, STYLE_MATCH_ICON))
		return false;
	return true;
}

bool style_match_id(const style_id_t *id, const style_match_patterns *patterns,
	style_match_subject *subject)
{
	/* A numeric mismatch decides this conjunction without string work. */
	if (id->flags.has_window_id && id->window_id != subject->window)
		return false;
	if (id->flags.is_compatibility_mode &&
	    !(patterns->universal & (1U << STYLE_MATCH_NAME)))
	{
		unsigned i;

		prepare_compatibility_fields(subject);

		for (i = 0; i <= STYLE_MATCH_NAME; i++)
		{
			if ((subject->compatibility_fields & (1U << i)) &&
			    match_pattern(id->name, patterns, STYLE_MATCH_NAME,
				subject, i))
				break;
		}
		if (i > STYLE_MATCH_NAME)
			return false;
	}
	unsigned fields =
		(id->flags.has_resource << STYLE_MATCH_RESOURCE) |
		(id->flags.has_class << STYLE_MATCH_CLASS) |
		(id->flags.has_name << STYLE_MATCH_NAME) |
		(id->flags.has_icon << STYLE_MATCH_ICON);
	unsigned literals = fields & patterns->literal;

	/* A failed exact field can decide the conjunction before a longer
	 * wildcard field scans its subject. Universal fields remain harmless
	 * in the second group, including their existing NULL semantics. */
	if (literals && !match_explicit_fields(id, patterns, subject, literals))
		return false;
	fields &= ~literals;
	if (fields && !match_explicit_fields(id, patterns, subject, fields))
		return false;
	return true;
}

/* These optional proofs must not add unbounded scanning/retry work.
 * Larger patterns remain conservative without limiting normal matching. */
#define STYLE_OVERLAP_PROOF_WORK 4096U

static bool pattern_lengths_may_overlap(const style_match_patterns *a,
	const style_match_patterns *b, unsigned field)
{
	unsigned bit = 1U << field;
	size_t a_minimum, b_minimum;

	/* Both wildcard families have decoded byte minima. A literal or
	 * fixed-width pattern also bounds the entire matching value. */
	if (!((a->literal | a->unescaped | a->escaped) & bit) ||
	    !((b->literal | b->unescaped | b->escaped) & bit))
		return true;
	a_minimum = a->literal & bit ? a->prefix[field] : a->minimum[field];
	b_minimum = b->literal & bit ? b->prefix[field] : b->minimum[field];
	if (((a->literal | a->fixed_width | a->escaped_fixed_width) & bit) &&
	    a_minimum < b_minimum)
		return false;
	if (((b->literal | b->fixed_width | b->escaped_fixed_width) & bit) &&
	    b_minimum < a_minimum)
		return false;
	return true;
}

/* Fixed-width metadata proves that each token consumes one value byte. */
static unsigned char fixed_pattern_byte(const char **pattern, bool *question)
{
	unsigned char byte = **pattern;

	*question = byte == '?';
	if (byte == '\\')
	{
		(*pattern)++;
		byte = **pattern;
		*question = false;
	}
	if (byte)
		(*pattern)++;
	return byte;
}

static bool fixed_patterns_may_overlap(const char *a,
	const style_match_patterns *ap, const char *b,
	const style_match_patterns *bp, unsigned field)
{
	unsigned bit = 1U << field;
	unsigned afixed =
		ap->literal | ap->fixed_width | ap->escaped_fixed_width;
	unsigned bfixed =
		bp->literal | bp->fixed_width | bp->escaped_fixed_width;
	const char *astart = a, *bstart = b;
	size_t a_length, b_length;

	/* Equal literal pairs were decided by the initial spans. Retain
	 * their existing fast path without decoding those bytes again. */
	if (!(afixed & bfixed & bit) ||
	    !((ap->fixed_width | ap->escaped_fixed_width |
	       bp->fixed_width | bp->escaped_fixed_width) & bit))
		return true;
	a_length = ap->literal & bit ? ap->prefix[field] : ap->minimum[field];
	b_length = bp->literal & bit ? bp->prefix[field] : bp->minimum[field];
	if (a_length != b_length)
		return false;
	if (a_length > STYLE_OVERLAP_PROOF_WORK)
		return true;
	for (size_t i = 0; i < a_length; i++)
	{
		bool aq, bq;
		unsigned char av, bv;

		/* Bound raw-byte work as well as decoded positions. A quoted
		 * token needs both bytes inside the proof allowance. */
		if ((size_t)(a - astart) >= STYLE_OVERLAP_PROOF_WORK ||
		    (size_t)(b - bstart) >= STYLE_OVERLAP_PROOF_WORK ||
		    (*a == '\\' &&
		     a - astart == STYLE_OVERLAP_PROOF_WORK - 1) ||
		    (*b == '\\' &&
		     b - bstart == STYLE_OVERLAP_PROOF_WORK - 1))
			return true;
		av = fixed_pattern_byte(&a, &aq);
		bv = fixed_pattern_byte(&b, &bq);
		/* A trailing escape describes no value. Quoted '?' and '*'
		 * are literals; an unquoted question can bridge a byte. */
		if (!av || !bv || (!aq && !bq && av != bv))
			return false;
	}
	return true;
}

static bool literal_member_may_overlap(const char *literal, size_t length,
	const char *pattern, const style_match_patterns *patterns,
	unsigned field)
{
	style_match_subject subject;
	size_t offset = patterns->suffix[field];
	size_t tail = patterns->suffix_length[field];
	size_t width;

	/* The final wildcard offset plus its literal tail is the raw width.
	 * Bound additions before forming the conservative cursor-work product.
	 * Division avoids overflow; normal matching is not capped. */
	if (length >= STYLE_OVERLAP_PROOF_WORK)
		return true;
	if (patterns->escaped & (1U << field))
	{
		/* Escaped suffix widths count decoded bytes. Bound the raw
		 * pattern before using it in the retry product. */
		for (width = 0; width < STYLE_OVERLAP_PROOF_WORK &&
		     pattern[width]; width++);
		if (width == STYLE_OVERLAP_PROOF_WORK)
			return true;
	}
	else
	{
		if (offset >= STYLE_OVERLAP_PROOF_WORK ||
		    tail >= STYLE_OVERLAP_PROOF_WORK - offset)
			return true;
		width = offset + tail;
	}
	if (width + 1 > STYLE_OVERLAP_PROOF_WORK / (length + 1))
		return true;
	/* The literal describes exactly one possible field value. Reuse the
	 * prepared matcher rather than treating wildcard bytes as literals.
	 * The subject borrows that value only for this synchronous proof. */
	style_match_subject_init(
		&subject, 0, literal, literal, literal, literal);
	subject.length[field] = length;
	subject.length_fields = 1U << field;
	return match_pattern(pattern, patterns, field, &subject, field);
}

/* Views borrow their pattern only for this synchronous optional proof. */
typedef struct
{
	const char *tail;
	size_t head_width, tail_width;
} overlap_spans;

static bool prepare_overlap_spans(const char *pattern, overlap_spans *spans)
{
	bool star = false;
	size_t raw = 0;

	spans->tail = pattern;
	spans->head_width = spans->tail_width = 0;
	while (raw < STYLE_OVERLAP_PROOF_WORK && pattern[raw])
	{
		if (pattern[raw] == '*')
		{
			star = true;
			spans->tail = pattern + raw + 1;
			spans->tail_width = 0;
			raw++;
			continue;
		}
		if (pattern[raw] == '\\')
		{
			if (++raw == STYLE_OVERLAP_PROOF_WORK || !pattern[raw])
				return false;
		}
		if (!star)
			spans->head_width++;
		spans->tail_width++;
		raw++;
	}
	/* Refuse an incomplete raw scan, including its unobserved terminator.
	 * Ordinary matching is independent of this optional allowance. */
	return raw < STYLE_OVERLAP_PROOF_WORK;
}

static bool decoded_spans_may_overlap(const char *a, const char *b,
	size_t width)
{
	for (size_t i = 0; i < width; i++)
	{
		bool aq, bq;
		unsigned char av = fixed_pattern_byte(&a, &aq);
		unsigned char bv = fixed_pattern_byte(&b, &bq);

		if (!aq && !bq && av != bv)
			return false;
	}
	return true;
}

static const char *advance_decoded_span(const char *pattern, size_t width)
{
	for (size_t i = 0; i < width; i++)
	{
		bool question;

		fixed_pattern_byte(&pattern, &question);
	}
	return pattern;
}

static bool zero_slack_may_overlap(const char *a, const char *b, size_t width)
{
	/* One pattern fixes the value length to the other's byte minimum.
	 * Every unquoted star must consume zero bytes in any common value. */
	for (size_t i = 0; i < width; i++)
	{
		bool aq, bq;
		unsigned char av, bv;

		while (*a == '*')
			a++;
		while (*b == '*')
			b++;
		av = fixed_pattern_byte(&a, &aq);
		bv = fixed_pattern_byte(&b, &bq);
		if (!aq && !bq && av != bv)
			return false;
	}
	return true;
}

static bool anchored_patterns_may_overlap(const char *a,
	const style_match_patterns *ap, const char *b,
	const style_match_patterns *bp, unsigned field)
{
	unsigned bit = 1U << field;
	bool afixed = (ap->fixed_width | ap->escaped_fixed_width) & bit;
	bool bfixed = (bp->fixed_width | bp->escaped_fixed_width) & bit;
	overlap_spans as, bs;
	size_t common;
	const char *at, *bt;
	bool decoded_edges, head, tail, slack;

	/* Literal membership and fixed pairs already have their own paths.
	 * Do not decode their verified bytes again. */
	if (((ap->literal | bp->literal) & bit) || (afixed && bfixed) ||
	    (ap->single_star & bp->single_star & bit) ||
	    !((ap->unescaped | ap->escaped) & bit) ||
	    !((bp->unescaped | bp->escaped) & bit))
		return true;
	/* Raw literal anchors already passed above. Decode only edges whose
	 * quotes/questions can expose further necessary positions, or a
	 * fixed-length pair whose stars have no room to expand. */
	decoded_edges = (ap->escaped | bp->escaped) & bit;
	head = decoded_edges || a[ap->prefix[field]] == '?' ||
		b[bp->prefix[field]] == '?';
	tail = decoded_edges ||
		(ap->suffix[field] && a[ap->suffix[field] - 1] == '?') ||
		(bp->suffix[field] && b[bp->suffix[field] - 1] == '?');
	slack = (afixed || bfixed) && ap->minimum[field] == bp->minimum[field];
	if (!head && !tail && !slack)
		return true;
	if (!prepare_overlap_spans(a, &as) || !prepare_overlap_spans(b, &bs))
		return true;
	common = as.head_width < bs.head_width ? as.head_width : bs.head_width;
	if (head && !decoded_spans_may_overlap(a, b, common))
		return false;
	common = as.tail_width < bs.tail_width ? as.tail_width : bs.tail_width;
	if (tail && common)
	{
		at = advance_decoded_span(as.tail, as.tail_width - common);
		bt = advance_decoded_span(bs.tail, bs.tail_width - common);
		if (!decoded_spans_may_overlap(at, bt, common))
			return false;
	}
	if (slack)
		return zero_slack_may_overlap(a, b, ap->minimum[field]);
	return true;
}

static bool pattern_spans_may_overlap(const char *a,
	const style_match_patterns *ap, const char *b,
	const style_match_patterns *bp, unsigned field)
{
	unsigned bit = 1U << field;
	size_t common, alength, blength;
	const char *asuffix, *bsuffix;

	if ((ap->universal | bp->universal) & bit)
		return true;
	common = ap->prefix[field] < bp->prefix[field] ?
		ap->prefix[field] : bp->prefix[field];
	if (common && memcmp(a, b, common) != 0)
		return false;
	if ((ap->literal & bp->literal & bit) &&
	    ap->prefix[field] != bp->prefix[field])
		return false;
	if (!pattern_lengths_may_overlap(ap, bp, field))
		return false;
	/* An escaped pattern has no prepared anchored suffix. Its initial
	 * unescaped prefix remains necessary; everything else is uncertain. */
	alength = ap->literal & bit ? ap->prefix[field] :
		ap->unescaped & bit ? ap->suffix_length[field] : 0;
	blength = bp->literal & bit ? bp->prefix[field] :
		bp->unescaped & bit ? bp->suffix_length[field] : 0;
	common = alength < blength ? alength : blength;
	if (common)
	{
		asuffix = a + (ap->literal & bit ? 0 : ap->suffix[field]);
		bsuffix = b + (bp->literal & bit ? 0 : bp->suffix[field]);
		if (memcmp(asuffix + alength - common,
		    bsuffix + blength - common, common) != 0)
			return false;
	}
	if (!fixed_patterns_may_overlap(a, ap, b, bp, field))
		return false;
	/* Fixed-width pairs were decided above. Bounded membership reuses
	 * the prepared matcher and its constant-stack escaped cursors. */
	if ((ap->literal & bit) && ((bp->unescaped | bp->escaped) & bit) &&
	    !((bp->fixed_width | bp->escaped_fixed_width) & bit))
		return literal_member_may_overlap(a, ap->prefix[field],
			b, bp, field);
	if ((bp->literal & bit) && ((ap->unescaped | ap->escaped) & bit) &&
	    !((ap->fixed_width | ap->escaped_fixed_width) & bit))
		return literal_member_may_overlap(b, bp->prefix[field],
			a, ap, field);
	return anchored_patterns_may_overlap(a, ap, b, bp, field);
}

/* False proves that no window can satisfy both conjunctions. Compatibility
 * selectors OR resource/class/name, so their name spans alone prove nothing.
 * Only constraints on the same explicit field can establish disjointness. */
bool style_match_ids_may_overlap(const style_id_t *a,
	const style_match_patterns *ap, const style_id_t *b,
	const style_match_patterns *bp)
{
	const char *av[STYLE_MATCH_FIELDS] = {
		a->resource, a->class, a->name, a->icon
	};
	const char *bv[STYLE_MATCH_FIELDS] = {
		b->resource, b->class, b->name, b->icon
	};
	bool shared[STYLE_MATCH_FIELDS] = {
		a->flags.has_resource && b->flags.has_resource,
		a->flags.has_class && b->flags.has_class,
		a->flags.has_name && b->flags.has_name,
		a->flags.has_icon && b->flags.has_icon
	};

	if (a->flags.has_window_id && b->flags.has_window_id &&
	    a->window_id != b->window_id)
		return false;
	for (unsigned i = 0; i < STYLE_MATCH_FIELDS; i++)
	{
		if (shared[i] && !pattern_spans_may_overlap(
		    av[i], ap, bv[i], bp, i))
			return false;
	}
	return true;
}
