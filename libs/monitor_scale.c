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

typedef __int128 monitor_scale_wide;

static bool
factor_is_valid(monitor_scale_factor factor)
{
	return factor.quarters >= MONITOR_SCALE_QUARTERS_MIN
	       && factor.quarters <= MONITOR_SCALE_QUARTERS_MAX;
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
