#ifndef FVWM_MONITOR_SCALE_H
#define FVWM_MONITOR_SCALE_H

#include "libs/monitor_topology.h"

#include <stdbool.h>
#include <stddef.h>

#define MONITOR_SCALE_QUARTERS_MIN 4U
#define MONITOR_SCALE_QUARTERS_MAX 16U
#define MONITOR_SCALE_DPI_BASE 96U
#define MONITOR_SCALE_DPI_MIN 50U
#define MONITOR_SCALE_DPI_MAX 500U
#define MONITOR_SCALE_DPI_AGREEMENT_PERCENT 10U

typedef struct
{
	unsigned int quarters;
} monitor_scale_factor;

typedef enum
{
	MONITOR_SCALE_SOURCE_OVERRIDE,
	MONITOR_SCALE_SOURCE_AUTOMATIC,
	MONITOR_SCALE_SOURCE_FALLBACK,
	MONITOR_SCALE_SOURCE_SAFE
} monitor_scale_source;

typedef enum
{
	MONITOR_SCALE_AUTO_VALID,
	MONITOR_SCALE_AUTO_MISSING_AXIS,
	MONITOR_SCALE_AUTO_X_IMPLAUSIBLE,
	MONITOR_SCALE_AUTO_Y_IMPLAUSIBLE,
	MONITOR_SCALE_AUTO_INCONSISTENT
} monitor_scale_auto_status;

typedef enum
{
	MONITOR_SCALE_BOUNDARY_NONE,
	MONITOR_SCALE_BOUNDARY_LOW,
	MONITOR_SCALE_BOUNDARY_HIGH
} monitor_scale_boundary;

typedef enum
{
	MONITOR_SCALE_REASON_EXACT_OVERRIDE,
	MONITOR_SCALE_REASON_VALID_AUTOMATIC,
	MONITOR_SCALE_REASON_AUTOMATIC_BOUND_LOW,
	MONITOR_SCALE_REASON_AUTOMATIC_BOUND_HIGH,
	MONITOR_SCALE_REASON_CONFIGURED_FALLBACK,
	MONITOR_SCALE_REASON_SAFE_DEFAULT
} monitor_scale_reason;

typedef enum
{
	MONITOR_SCALE_CONVERSION_OK,
	MONITOR_SCALE_CONVERSION_OVERFLOW,
	MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT
} monitor_scale_conversion_status;

typedef enum
{
	MONITOR_SCALE_MATCH_NONE,
	MONITOR_SCALE_MATCH_UNIQUE,
	MONITOR_SCALE_MATCH_AMBIGUOUS
} monitor_scale_match_status;

typedef struct
{
	monitor_scale_factor	   factor;
	monitor_scale_source	   source;
	monitor_scale_match_status match_status;
	monitor_scale_auto_status  auto_status;
	monitor_scale_boundary	   boundary;
	monitor_scale_reason	   reason;
} monitor_scale_result;

#define MONITOR_SCALE_RESULT_SAFE_INIT                                 \
	{{MONITOR_SCALE_QUARTERS_MIN}, MONITOR_SCALE_SOURCE_SAFE,      \
	    MONITOR_SCALE_MATCH_NONE, MONITOR_SCALE_AUTO_MISSING_AXIS, \
	    MONITOR_SCALE_BOUNDARY_NONE, MONITOR_SCALE_REASON_SAFE_DEFAULT}

bool
monitor_scale_parse_factor(const char *text, monitor_scale_factor *factor);
bool
monitor_scale_format_factor(
    monitor_scale_factor factor, char *buffer, size_t buffer_size);
monitor_scale_result
monitor_scale_resolve(const monitor_topology_monitor *monitor,
    const monitor_scale_factor			     *override_factor,
    const monitor_scale_factor			     *fallback_factor);

/* Always convert from the retained exact source, never a rounded result. */
monitor_scale_conversion_status
monitor_scale_apply(int source, monitor_scale_factor factor, int *converted);
monitor_scale_conversion_status
monitor_scale_remove(int source, monitor_scale_factor factor, int *converted);

#endif
