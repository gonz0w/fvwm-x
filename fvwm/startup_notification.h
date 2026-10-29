#ifndef FVWM_STARTUP_NOTIFICATION_H
#define FVWM_STARTUP_NOTIFICATION_H

#include "libs/fvwm_x11.h"
#include "libs/cJSON.h"
#include "query_json.h"

struct FvwmWindow;

void startup_notification_initialize(void);
Bool startup_notification_client_message(const XClientMessageEvent *event);
void startup_notification_window_observed(struct FvwmWindow *window);
void startup_notification_property(struct FvwmWindow *window, Atom property);
void startup_notification_window_registered(struct FvwmWindow *window);
void startup_notification_window_retiring(struct FvwmWindow *window);
void startup_notification_raw_property(const XPropertyEvent *event);
Bool startup_notification_destroyed(const XDestroyWindowEvent *event);
void startup_notification_outer_boundary(void);
Bool startup_notification_outer_pending(void);
/* Pure local state/deadline service; safe during borrowed interactive state. */
void startup_notification_service(void);
int startup_notification_next_ms(void);
cJSON *startup_notification_query(const struct fvwm_query_spec *spec,
	const char **error);

#endif
