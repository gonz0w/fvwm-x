/* -*-c-*- */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>
 */

#include "config.h"

#include "libs/monitor_scale.h"

#include <limits.h>
#include <string.h>

typedef __int128 monitor_scale_wide;

static bool
factor_is_valid(monitor_scale_factor factor)
{
	return factor.quarters >= MONITOR_SCALE_QUARTERS_MIN
	       && factor.quarters <= MONITOR_SCALE_QUARTERS_MAX;
}

static const char *const canonical_factors[] = {
    "1",
    "1.25",
    "1.5",
    "1.75",
    "2",
    "2.25",
    "2.5",
    "2.75",
    "3",
    "3.25",
    "3.5",
    "3.75",
    "4",
};

bool
monitor_scale_parse_factor(const char *text, monitor_scale_factor *factor)
{
	size_t i;

	if (factor == NULL) {
		return false;
	}
	factor->quarters = MONITOR_SCALE_QUARTERS_MIN;
	if (text == NULL) {
		return false;
	}
	for (i = 0;
	    i < sizeof(canonical_factors) / sizeof(canonical_factors[0]); i++) {
		if (strcmp(text, canonical_factors[i]) == 0) {
			factor->quarters = MONITOR_SCALE_QUARTERS_MIN + i;
			return true;
		}
	}

	return false;
}

bool
monitor_scale_format_factor(
    monitor_scale_factor factor, char *buffer, size_t buffer_size)
{
	const char *canonical;
	size_t	    length;

	if (buffer == NULL || buffer_size == 0) {
		return false;
	}
	buffer[0] = '\0';
	if (!factor_is_valid(factor)) {
		return false;
	}
	canonical =
	    canonical_factors[factor.quarters - MONITOR_SCALE_QUARTERS_MIN];
	length = strlen(canonical);
	if (length >= buffer_size) {
		return false;
	}
	memcpy(buffer, canonical, length + 1);

	return true;
}

static monitor_scale_auto_status
automatic_factor(const monitor_topology_monitor *monitor,
    monitor_scale_factor *factor, monitor_scale_boundary *boundary)
{
	monitor_scale_wide x_numerator;
	monitor_scale_wide x_denominator;
	monitor_scale_wide y_numerator;
	monitor_scale_wide y_denominator;
	monitor_scale_wide x_cross;
	monitor_scale_wide y_cross;
	monitor_scale_wide difference;
	monitor_scale_wide mean_numerator;
	monitor_scale_wide scale_denominator;
	monitor_scale_wide quarters;
	monitor_scale_wide remainder;

	*factor	  = (monitor_scale_factor){MONITOR_SCALE_QUARTERS_MIN};
	*boundary = MONITOR_SCALE_BOUNDARY_NONE;
	if (monitor == NULL || monitor->width <= 0 || monitor->height <= 0
	    || monitor->mm_width <= 0 || monitor->mm_height <= 0) {
		return MONITOR_SCALE_AUTO_MISSING_AXIS;
	}

	x_numerator   = (monitor_scale_wide)127 * monitor->width;
	x_denominator = (monitor_scale_wide)5 * monitor->mm_width;
	y_numerator   = (monitor_scale_wide)127 * monitor->height;
	y_denominator = (monitor_scale_wide)5 * monitor->mm_height;
	if (x_numerator
		< (monitor_scale_wide)MONITOR_SCALE_DPI_MIN * x_denominator
	    || x_numerator > (monitor_scale_wide)MONITOR_SCALE_DPI_MAX
				 * x_denominator) {
		return MONITOR_SCALE_AUTO_X_IMPLAUSIBLE;
	}
	if (y_numerator
		< (monitor_scale_wide)MONITOR_SCALE_DPI_MIN * y_denominator
	    || y_numerator > (monitor_scale_wide)MONITOR_SCALE_DPI_MAX
				 * y_denominator) {
		return MONITOR_SCALE_AUTO_Y_IMPLAUSIBLE;
	}

	x_cross	   = x_numerator * y_denominator;
	y_cross	   = y_numerator * x_denominator;
	difference = x_cross >= y_cross ? x_cross - y_cross : y_cross - x_cross;
	if ((monitor_scale_wide)(2 * MONITOR_SCALE_DPI_AGREEMENT_PERCENT)
		* difference
	    > x_cross + y_cross) {
		return MONITOR_SCALE_AUTO_INCONSISTENT;
	}

	mean_numerator = x_cross + y_cross;
	scale_denominator =
	    (monitor_scale_wide)48 * x_denominator * y_denominator;
	quarters  = mean_numerator / scale_denominator;
	remainder = mean_numerator % scale_denominator;
	if (2 * remainder >= scale_denominator) {
		quarters++;
	}
	if (quarters < MONITOR_SCALE_QUARTERS_MIN) {
		quarters  = MONITOR_SCALE_QUARTERS_MIN;
		*boundary = MONITOR_SCALE_BOUNDARY_LOW;
	} else if (quarters > MONITOR_SCALE_QUARTERS_MAX) {
		quarters  = MONITOR_SCALE_QUARTERS_MAX;
		*boundary = MONITOR_SCALE_BOUNDARY_HIGH;
	}
	factor->quarters = (unsigned int)quarters;

	return MONITOR_SCALE_AUTO_VALID;
}

monitor_scale_result
monitor_scale_resolve(const monitor_topology_monitor *monitor,
    const monitor_scale_factor			     *override_factor,
    const monitor_scale_factor			     *fallback_factor)
{
	monitor_scale_result result = MONITOR_SCALE_RESULT_SAFE_INIT;
	monitor_scale_factor automatic;

	result.auto_status =
	    automatic_factor(monitor, &automatic, &result.boundary);
	if (override_factor != NULL && factor_is_valid(*override_factor)) {
		result.factor	    = *override_factor;
		result.source	    = MONITOR_SCALE_SOURCE_OVERRIDE;
		result.match_status = MONITOR_SCALE_MATCH_UNIQUE;
		result.reason	    = MONITOR_SCALE_REASON_EXACT_OVERRIDE;
		return result;
	}
	if (result.auto_status == MONITOR_SCALE_AUTO_VALID) {
		result.factor = automatic;
		result.source = MONITOR_SCALE_SOURCE_AUTOMATIC;
		result.reason = result.boundary == MONITOR_SCALE_BOUNDARY_LOW
				    ? MONITOR_SCALE_REASON_AUTOMATIC_BOUND_LOW
				: result.boundary == MONITOR_SCALE_BOUNDARY_HIGH
				    ? MONITOR_SCALE_REASON_AUTOMATIC_BOUND_HIGH
				    : MONITOR_SCALE_REASON_VALID_AUTOMATIC;
		return result;
	}
	if (fallback_factor != NULL && factor_is_valid(*fallback_factor)) {
		result.factor = *fallback_factor;
		result.source = MONITOR_SCALE_SOURCE_FALLBACK;
		result.reason = MONITOR_SCALE_REASON_CONFIGURED_FALLBACK;
	}

	return result;
}

static monitor_scale_conversion_status
convert_ratio(int source, unsigned int numerator, unsigned int denominator,
    int *converted)
{
	monitor_scale_wide value;
	monitor_scale_wide magnitude;
	monitor_scale_wide quotient;
	monitor_scale_wide remainder;
	monitor_scale_wide maximum;
	bool		   negative;

	if (converted == NULL) {
		return MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT;
	}
	*converted = 0;
	if (denominator == 0) {
		return MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT;
	}

	value	  = (monitor_scale_wide)source * numerator;
	negative  = value < 0;
	magnitude = negative ? -value : value;
	quotient  = magnitude / denominator;
	remainder = magnitude % denominator;
	if (2 * remainder >= denominator) {
		quotient++;
	}
	maximum = negative ? (monitor_scale_wide)INT_MAX + 1 : INT_MAX;
	if (quotient > maximum) {
		return MONITOR_SCALE_CONVERSION_OVERFLOW;
	}
	if (negative && quotient == (monitor_scale_wide)INT_MAX + 1) {
		*converted = INT_MIN;
	} else if (negative) {
		*converted = -(int)quotient;
	} else {
		*converted = (int)quotient;
	}

	return MONITOR_SCALE_CONVERSION_OK;
}

monitor_scale_conversion_status
monitor_scale_apply(int source, monitor_scale_factor factor, int *converted)
{
	if (converted != NULL) {
		*converted = 0;
	}
	if (!factor_is_valid(factor)) {
		return MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT;
	}

	return convert_ratio(source, factor.quarters, 4, converted);
}

monitor_scale_conversion_status
monitor_scale_remove(int source, monitor_scale_factor factor, int *converted)
{
	if (converted != NULL) {
		*converted = 0;
	}
	if (!factor_is_valid(factor)) {
		return MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT;
	}

	return convert_ratio(source, 4, factor.quarters, converted);
}
