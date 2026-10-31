#ifndef FVWMLIB_MODULE_STARTUP_H
#define FVWMLIB_MODULE_STARTUP_H

#include <stdbool.h>

#define MODULE_STARTUP_SECONDS 5U

/* This isolated process owns SIGALRM and its alarm timer. Invalid intervals
 * leave signal/timer state unchanged. Success restores default termination,
 * unblocks SIGALRM and replaces any inherited alarm. No handler performs work.
 * Signal delivery requests termination; a kernel-blocked exit can still lag. */
bool module_startup_begin(unsigned int seconds);
/* Cancel only this process-local startup timer; do not restore inherited
 * handlers or masks after optional startup has completed. */
void module_startup_finish(void);

#endif
