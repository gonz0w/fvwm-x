/* -*-c-*- */

#ifndef FVWM_MODULE_INTERFACE_H
#define FVWM_MODULE_INTERFACE_H

#include "libs/Module.h"
#include "module_list.h"

struct fmodule;
struct fmodule_input;

/* Packet sending functions */
void BroadcastPacket(unsigned long event_type, unsigned long num_datum, ...);
void BroadcastConfig(unsigned long event_type, const FvwmWindow *t);
/* Refresh unsent snapshots even when rollback caused a no-op frame update. */
void module_config_refresh(const FvwmWindow *window);
void BroadcastName(
	unsigned long event_type, unsigned long data1, unsigned long data2,
	unsigned long data3, const char *name);
void BroadcastWindowIconNames(FvwmWindow *t, Bool window, Bool icon);
void BroadcastFvwmPicture(
	unsigned long event_type, unsigned long data1, unsigned long data2,
	unsigned long data3, FvwmPicture *picture, char *name);
void BroadcastPropertyChange(
	unsigned long argument, unsigned long data1,
	unsigned long data2, char *string);
void BroadcastColorset(int n);
void BroadcastConfigInfoString(char *string);
void BroadcastMonitorList(fmodule *);
/* Publish finalized UI scale without legacy DesktopSize side effects. */
void broadcast_monitor_ui_scale(void);
void BroadcastDesktopConfiguration(fmodule *);
void broadcast_ignore_modifiers(void);
void SendPacket(
	struct fmodule *module, unsigned long event_type,
	unsigned long num_datum, ...);
void SendConfig(
	struct fmodule *module, unsigned long event_type, const FvwmWindow *t);
void SendFvwmPicture(
	struct fmodule *module, unsigned long event_type, unsigned long data1,
	unsigned long data2, unsigned long data3, FvwmPicture *picture, char *name);
void SendName(
	struct fmodule *module, unsigned long event_type, unsigned long data1,
	unsigned long data2, unsigned long data3, const char *name);

/* The source packet is never a separate synchronization barrier; enqueue the
 * complete pair before honoring the existing config-info synchronization. */
void send_config_source(struct fmodule *module, unsigned long key,
	const char *source, const char *config);

/* command queue - module input */


/* Takes ownership of an admitted input returned by module_receive_queued. */
void module_input_enqueue(struct fmodule_input *input);
void module_input_execute(struct fmodule_input *input);

Bool ExecuteCommandQueue(void);
Bool ExecuteInteractionCommandQueue(void);
Bool interaction_command_ready(const char *command, fmodule *origin);
void ModuleCommandQueueAfterPoll(void);

#endif /* MODULE_INTERFACE_H */
