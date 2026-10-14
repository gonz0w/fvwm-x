#ifndef FVWM_STACK_SORT_H
#define FVWM_STACK_SORT_H

#include "fvwm.h"

/* Stable descending rank sort of a valid temporary stack ring. No X work,
 * callbacks, allocation or retained references; all links are circular on
 * return. Already-sorted rings retain their exact links. */
void fvwm_stack_sort_ring(FvwmWindow *ring);

#endif
