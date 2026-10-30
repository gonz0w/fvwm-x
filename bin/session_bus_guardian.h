#ifndef FVWM_SESSION_BUS_GUARDIAN_H
#define FVWM_SESSION_BUS_GUARDIAN_H

/* Private helper branch: exactly bus-guardian and one absolute daemon path.
 * FD3 is the readable lifetime FIFO; stdout carries address and W/U records. */
int session_bus_guardian_main(int argc, char **argv);

#endif
