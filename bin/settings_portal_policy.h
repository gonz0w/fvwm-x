#ifndef FVWM_SETTINGS_PORTAL_POLICY_H
#define FVWM_SETTINGS_PORTAL_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SP_SETTINGS_MAX 65536U
#define SP_RECORDS_MAX 256U
#define SP_PATTERNS_MAX 16U
#define SP_PATTERN_MAX 128U
#define SP_NAMESPACE "org.freedesktop.appearance"
#define SP_KEY "reduced-motion"

typedef struct
{
	uint32_t serial;
	uint32_t reduced_motion;
} sp_snapshot;

/* Complete owner-local snapshot; failure clears every output. Serials are
 * observations, never a global/increasing freshness test across owners. */
bool sp_decode(const unsigned char *data, size_t length, sp_snapshot *snapshot);
/* An empty pattern list selects all. A trailing .* selects sections below
 * its prefix; other wildcard placements are invalid. */
bool sp_pattern_valid(const char *pattern);
bool sp_namespace_matches(const char *const *patterns, size_t count);
/* Commit unavailable/malformed sources as no preference (0). Return whether
 * the public value changed; owner/serial-only changes do not churn signals. */
bool sp_commit(uint32_t *current, const sp_snapshot *snapshot);

#endif
