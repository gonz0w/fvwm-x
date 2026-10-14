#ifndef FVWMLIB_MODULE_X_SERVICE_H
#define FVWMLIB_MODULE_X_SERVICE_H

#include <stdbool.h>
#include <time.h>

#define MODULE_X_EVENT_BUDGET 32
#define MODULE_X_SERVICE_NS 5000000L

/* One event preserves progress after scheduler preemption. Invalid or
 * regressing clock values end the batch; coalesced events count too. */
bool module_x_service_available(unsigned int serviced,
	const struct timespec *started, const struct timespec *now);

#endif
