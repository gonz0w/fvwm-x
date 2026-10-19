#ifndef FVWM_FUNCTABLE_COMPLEX_TYPES_H
#define FVWM_FUNCTABLE_COMPLEX_TYPES_H

#include "functable.h"
#include "functable_complex.h"

/* Records are owned by the command engine and complex-function store. */
typedef struct FunctionItem
{
	struct FvwmFunction *func;       /* the function this item is in */
	struct FunctionItem *next_item;  /* next function item */
	char condition;                  /* the character string displayed on
					  * left*/
	char *action;                    /* action to be performed */
	short type;                      /* type of built in function */
	func_flags_t flags;
} FunctionItem;

struct FvwmFunction
{
	struct FvwmFunction *next_func;  /* next in list of root menus */
	FunctionItem *first_item;        /* first item in function */
	FunctionItem *last_item;         /* last item in function */
	char *name;                      /* function name */
	int use_depth;
	int num_items;
};

#endif
