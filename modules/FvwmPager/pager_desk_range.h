#ifndef FVWM_PAGER_DESK_RANGE_H
#define FVWM_PAGER_DESK_RANGE_H
#include <stdbool.h>
typedef struct {
	int first;
	int last;
	int count;
} pager_desk_range;
bool pager_prepare_desk_range(const char *first, const char *last,
	int limit, pager_desk_range *result);
/* Return -1 for a desk outside the displayed range, without signed wrap. */
int pager_desk_index(int desk, int first, int count);
#endif
