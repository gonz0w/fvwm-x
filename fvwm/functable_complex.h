/* -*-c-*- */

#ifndef FVWM_FUNCTABLE_COMPLEX_H
#define FVWM_FUNCTABLE_COMPLEX_H

/* ---------------------------- included header files ---------------------- */

/* ---------------------------- global definitions ------------------------- */

/* ---------------------------- global macros ------------------------------ */

/* ---------------------------- type definitions --------------------------- */

typedef struct FvwmFunction FvwmFunction;

/* Types of events for the FUNCTION builtin */
typedef enum
{
	CF_IMMEDIATE =      'i',
	CF_LATE_IMMEDIATE = 'j',
	CF_MOTION =         'm',
	CF_HOLD =           'h',
	CF_CLICK =          'c',
	CF_DOUBLE_CLICK =   'd',
	CF_TIMEOUT =        '-',
	CF_CANCELLED =      0
} cfunc_action_t;

/* ---------------------------- exported variables (globals) --------------- */

/* ---------------------------- interface functions ------------------------ */

FvwmFunction *NewFvwmFunction(const char *name);
void DestroyFunction(FvwmFunction *func);
FvwmFunction *find_complex_function(const char *function_name);
Bool functions_is_complex_function(const char *function_name);

#endif /* FVWM_FUNCTABLE_COMPLEX_H */
