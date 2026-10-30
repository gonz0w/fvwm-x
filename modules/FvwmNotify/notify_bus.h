#ifndef FVWM_NOTIFY_BUS_H
#define FVWM_NOTIFY_BUS_H

#include "notify_model.h"

typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;

typedef struct
{
	DBusConnection *connection;
	notify_model model;
	bool dirty, failed, pending;
} notify_bus;

/* Only an explicit local Unix bus address is accepted; no autolaunch, queueing,
 * replacement or service activation. Startup authentication is isolated in this
 * optional module process; no managed-window/core state is borrowed. */
/* Side-effect-free admission helpers for native protocol unit coverage. */
bool notify_bus_address_valid(const char *address);
bool notify_bus_parse_notify(DBusMessage *message, notify_request *request);
bool notify_bus_open(notify_bus *bus, const char *address);
void notify_bus_destroy(notify_bus *bus);
int notify_bus_fd(const notify_bus *bus);
short notify_bus_events(const notify_bus *bus);
/* Nonblocking IO, <=32 incoming messages and <=32 expirations per call. Output
 * is capped at 64 KiB; inability to retain a reply terminates this owner rather
 * than silently applying an unacknowledged command. */
bool notify_bus_service(notify_bus *bus, double now);
bool notify_bus_close(notify_bus *bus, uint32_t id, uint32_t reason);
bool notify_bus_action(notify_bus *bus, uint32_t id, unsigned int action);
void notify_bus_dnd(notify_bus *bus, bool enabled);
void notify_bus_dismiss_all(notify_bus *bus);

#endif
