#ifndef FVWMLIB_MODULE_MONITOR_H
#define FVWMLIB_MODULE_MONITOR_H

#include <stdbool.h>
#include <stddef.h>

#define MODULE_MONITOR_VALUES 14

typedef struct {
	int x;
	int y;
	int width;
	int height;
} module_monitor_area;

/* Decimal-prefix parsing; publish the caller's temporary values only on success. */
bool module_parse_decimal_integers(const char *text, int *values, size_t count);
/* Quote only names requiring token protection; never emit a truncated record. */
bool module_monitor_format(char *output, size_t capacity, const char *name,
	const int values[MODULE_MONITOR_VALUES]);
/* Prepare checked inset geometry before publishing a rearrangement box. */
bool module_monitor_prepare_area(const int values[MODULE_MONITOR_VALUES],
	const module_monitor_area *initial, bool ignore_work_area,
	module_monitor_area *result);

#endif
