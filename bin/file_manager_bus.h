#ifndef FVWM_FILE_MANAGER_BUS_H
#define FVWM_FILE_MANAGER_BUS_H

/* The caller validates the inherited address with session_bus local_address.
 * This branch owns only its private connection, never the bus daemon. */
int file_manager_bus_main(const char *address);
int file_manager_bus_unavailable(const char *detail);

#endif
