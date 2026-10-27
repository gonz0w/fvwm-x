#ifndef FVWM_CONFIG_ORIGIN_H
#define FVWM_CONFIG_ORIGIN_H

#include <stddef.h>

#define FVWM_CONFIG_SOURCE_MAX_BYTES 1536
#define FVWM_CONFIG_SOURCE_MAX_INCLUDE 16

typedef struct fvwm_config_origin fvwm_config_origin;

/* Immutable owned snapshots. NULL means unavailable, never current Read.
 * A file beneath an unknown/generated/function include site is unavailable;
 * never represent an invocation frame as an exact literal Read chain. */
fvwm_config_origin *fvwm_config_origin_file(const char *path,
	size_t first, size_t last, const fvwm_config_origin *parent);
fvwm_config_origin *fvwm_config_origin_derive(
	const fvwm_config_origin *site, const char *kind, const char *reason);
fvwm_config_origin *fvwm_config_origin_retain(const fvwm_config_origin *origin);
void fvwm_config_origin_release(const fvwm_config_origin *origin);
const char *fvwm_config_origin_json(const fvwm_config_origin *origin);

#endif
