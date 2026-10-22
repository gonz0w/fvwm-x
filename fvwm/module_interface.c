/* -*-c-*- */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>
 */

/*
 *
 * code for talking with fvwm modules.
 *
 */
#include "config.h"
#include "fvwm/module_records.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "libs/log.h"
#include "libs/ftime.h"
#include "libs/fvwmlib.h"
#include "libs/ColorUtils.h"
#include "libs/Parse.h"
#include "libs/Strings.h"
#include "libs/module_monitor.h"
#include "libs/wild.h"
#include "libs/FEvent.h"
#include "fvwm.h"
#include "externs.h"

#include "functions.h"
#include "bindings.h"
#include "misc.h"
#include "screen.h"
#include "module_interface.h"
#include "module_list.h"
#include "event_loop_policy.h"
#include "events.h"
#include "geometry.h"
#include "libs/fvwmsignal.h"
#include "decorations.h"
#include "ewmh.h"
#include "commands.h"
#include "module_queue.h"
#include "query_json.h"
#include "libs/vpacket.h"
#include "libs/monitor_types.h"
#include "libs/picture_types.h"
/* A queue of commands from the modules */
static module_command_queue cqueue;
static fvwm_queue_drain_state cqueue_drain_state =
	FVWM_QUEUE_DRAIN_STATE_INIT;

static void send_monitor_info(fmodule *);

static unsigned long
make_vpacket(unsigned long *body, unsigned long event_type,
	     unsigned long num, va_list ap)
{
	unsigned long *bp = body;

	/* truncate long packets */
	if (num > FvwmPacketMaxSize - FvwmPacketHeaderSize)
	{
		num = FvwmPacketMaxSize - FvwmPacketHeaderSize;
	}
	*(bp++) = START_FLAG;
	*(bp++) = event_type;
	*(bp++) = num+FvwmPacketHeaderSize;
	*(bp++) = fev_get_evtime();

	for (; num > 0; --num)
	{
		*(bp++) = va_arg(ap, unsigned long);
	}

	return body[2];
}



void SendPacket(
	fmodule *module, unsigned long event_type, unsigned long num_datum,
	...)
{
	unsigned long body[FvwmPacketMaxSize];
	va_list ap;
	unsigned long length;

	va_start(ap, num_datum);
	length = make_vpacket(body, event_type, num_datum, ap);
	va_end(ap);
	PositiveWrite(
		module, body,
		length * sizeof(body[0]));

	return;
}

void BroadcastPacket(unsigned long event_type, unsigned long num_datum, ...)
{
	unsigned long body[FvwmPacketMaxSize];
	va_list ap;
	unsigned long length;
	fmodule_list_itr moditr;
	fmodule *module;

	va_start(ap,num_datum);
	length = make_vpacket(body, event_type, num_datum, ap);
	va_end(ap);
	module_list_itr_init(&moditr);
	while ( (module = module_list_itr_next(&moditr)) != NULL)
	{
		PositiveWrite(
			module, body,
			length * sizeof(body[0]));
	}

	return;
}


static void set_allowed_actions(action_flags *actions, const FvwmWindow *fw)
{
	actions->is_movable = is_function_allowed(
		F_MOVE, NULL, fw, RQORIG_PROGRAM_US, False);
	actions->is_deletable = is_function_allowed(
		F_DELETE, NULL, fw, RQORIG_PROGRAM_US, False);
	actions->is_destroyable = is_function_allowed(
		F_DESTROY, NULL, fw, RQORIG_PROGRAM_US, False);
	actions->is_closable = is_function_allowed(
		F_CLOSE, NULL, fw, RQORIG_PROGRAM_US, False);
	actions->is_maximizable = is_function_allowed(
		F_MAXIMIZE, NULL, fw, RQORIG_PROGRAM_US, False);
	actions->is_resizable = is_function_allowed(
		F_RESIZE, NULL, fw, RQORIG_PROGRAM_US, False);
	actions->is_iconifiable = is_function_allowed(
		F_ICONIFY, NULL, fw, RQORIG_PROGRAM_US, False);

}

/* Configure records use the same native layout as their shipped consumers. */
_Static_assert(sizeof(ConfigWinPacket) <=
	(FvwmPacketMaxSize - FvwmPacketHeaderSize) * sizeof(unsigned long),
	"Configure record exceeds module packet capacity");

static int make_config_packet(unsigned long *body, unsigned long event_type,
	const FvwmWindow *fw)
{
	ConfigWinPacket packet;
	const size_t words = FvwmPacketHeaderSize +
		(sizeof(packet) + sizeof(*body) - 1) / sizeof(*body);

	/* Initialize native structure and final word padding before publication. */
	memset(&packet, 0, sizeof(packet));
	packet.w = FW_W(fw);
	packet.frame = FW_W_FRAME(fw);
	packet.fvwmwin = (unsigned long *)fw;
	packet.frame_x = fw->g.frame.x;
	packet.frame_y = fw->g.frame.y;
	packet.frame_width = fw->g.frame.width;
	packet.frame_height = fw->g.frame.height;
	packet.desk = fw->Desk;
	packet.monitor_id = fw->m->si->rr_output;
	packet.layer = fw->layer;
	packet.hints_base_width = fw->hints.base_width;
	packet.hints_base_height = fw->hints.base_height;
	packet.hints_width_inc = fw->hints.width_inc;
	packet.hints_height_inc = fw->hints.height_inc;
	packet.orig_hints_width_inc = fw->orig_hints.width_inc;
	packet.orig_hints_height_inc = fw->orig_hints.height_inc;
	packet.hints_min_width = fw->hints.min_width;
	packet.hints_min_height = fw->hints.min_height;
	packet.hints_max_width = fw->hints.max_width;
	packet.hints_max_height = fw->hints.max_height;
	packet.icon_w = FW_W_ICON_TITLE(fw);
	packet.icon_pixmap_w = FW_W_ICON_PIXMAP(fw);
	packet.hints_win_gravity = fw->hints.win_gravity;
	packet.TextPixel = fw->colors.fore;
	packet.BackPixel = fw->colors.back;
	packet.ewmh_hint_layer = fw->ewmh_hint_layer;
	packet.ewmh_hint_desktop = fw->ewmh_hint_desktop;
	packet.ewmh_window_type = fw->ewmh_window_type;
	packet.title_height = fw->title_thickness;
	packet.border_width = fw->boundary_width;
	memcpy(&packet.flags, &fw->flags, sizeof(packet.flags));
	set_allowed_actions(&packet.allowed_actions, fw);

	body[0] = START_FLAG;
	body[1] = event_type;
	body[2] = words;
	body[3] = fev_get_evtime();
	memset(body + FvwmPacketHeaderSize, 0,
		(words - FvwmPacketHeaderSize) * sizeof(*body));
	memcpy(body + FvwmPacketHeaderSize, &packet, sizeof(packet));
	return words * sizeof(*body);
}

void SendConfig(fmodule *module, unsigned long event_type, const FvwmWindow *fw)
{
	unsigned long body[FvwmPacketMaxSize];
	int size = make_config_packet(body, event_type, fw);

	PositiveWrite(module, body, size);
}

void BroadcastConfig(unsigned long event_type, const FvwmWindow *fw)
{
	unsigned long body[FvwmPacketMaxSize];
	int size = make_config_packet(body, event_type, fw);
	fmodule_list_itr iterator;
	fmodule *module;

	module_list_itr_init(&iterator);
	while ((module = module_list_itr_next(&iterator)) != NULL)
	{
		PositiveWrite(module, body, size);
	}
}

void module_config_refresh(const FvwmWindow *window)
{
	fmodule_list_itr iterator;
	fmodule *module;

	module_list_itr_init(&iterator);
	while ((module = module_list_itr_next(&iterator)) != NULL)
	{
		if (module_config_has_window(module, FW_W(window)))
		{
			SendConfig(module, M_CONFIGURE_WINDOW, window);
		}
	}
}

static int make_named_packet(
	unsigned long *body, unsigned long event_type,
	const char *name, int num, ...)
{
	va_list ap;
	size_t name_length;
	size_t available;
	int length;

	if (name == NULL || num < 0 ||
	    num >= FvwmPacketMaxSize - FvwmPacketHeaderSize)
	{
		return 0;
	}
	available = (FvwmPacketMaxSize - FvwmPacketHeaderSize - num) *
		sizeof(*body);
	/* Only bytes representable in this packet affect its length. */
	name_length = strnlen(name, available - 1);
	length = FvwmPacketHeaderSize + num + name_length / sizeof(*body) + 1;
	va_start(ap, num);
	make_vpacket(body, event_type, num, ap);
	va_end(ap);
	available = (length - FvwmPacketHeaderSize - num) * sizeof(*body);
	memset(&body[FvwmPacketHeaderSize + num], 0, available);
	memcpy(&body[FvwmPacketHeaderSize + num], name, name_length);
	body[2] = length;
	return length;
}

void SendName(
	fmodule *module, unsigned long event_type,
	unsigned long data1,unsigned long data2, unsigned long data3,
	const char *name)
{
	unsigned long body[FvwmPacketMaxSize];
	int l;

	if (name == NULL)
	{
		return;
	}
	l = make_named_packet(body, event_type, name, 3, data1, data2, data3);
	PositiveWrite(module, body, l*sizeof(unsigned long));

	return;
}

void BroadcastName(
	unsigned long event_type,
	unsigned long data1, unsigned long data2, unsigned long data3,
	const char *name)
{
	unsigned long body[FvwmPacketMaxSize];
	int l;
	fmodule_list_itr moditr;
	fmodule *module;

	if (name == NULL)
	{
		return;
	}
	l = make_named_packet(body, event_type, name, 3, data1, data2, data3);
	module_list_itr_init(&moditr);
	while ( (module = module_list_itr_next(&moditr)) != NULL)
	{
		PositiveWrite(module, body, l*sizeof(unsigned long));
	}

	return;
}

void BroadcastDesktopConfiguration(fmodule *send)
{
	char	name[256];

	snprintf(name, sizeof(name), "DesktopConfiguration %d %d",
	    monitor_mode, is_tracking_shared);

	SendName(send, M_CONFIG_INFO, 0, 0, 0, name);
}

static
void send_monitor_info(fmodule *send)
{
	struct monitor	*m;
	boundingbox	 r;
	char name[(FvwmPacketBodyMaxSize - 3) * sizeof(unsigned long)];

	RB_FOREACH(m, monitors, &monitor_q) {
		r = get_ewmhc_boundingbox(m);
		const int values[MODULE_MONITOR_VALUES] = {m->flags, m->dx, m->dy,
			m->virtual_scr.Vx, m->virtual_scr.Vy,
			m->virtual_scr.VxMax, m->virtual_scr.VyMax,
			m->virtual_scr.CurrentDesk, monitor_get_all_widths(),
			monitor_get_all_heights(), r.left, r.right, r.top, r.bottom};
		if (!module_monitor_format(name, sizeof(name), m->si->name, values))
		{
			fvwm_debug(__func__, "Monitor record cannot fit its native packet");
			continue;
		}

		SendName(send, M_CONFIG_INFO, 0, 0, 0, name);
	}
}

void BroadcastMonitorList(fmodule *this)
{
	char action[256];
	struct monitor	*m;
	fmodule_list_itr moditr;
	fmodule *module;

	module_list_itr_init(&moditr);

	if (this != NULL) {
		/*
		 * We've been requested to send this information to a specific
		 * module only.
		 */
		send_monitor_info(this);
		BroadcastDesktopConfiguration(this);
		goto out;
	}

	while ((module = module_list_itr_next(&moditr)) != NULL) {
		send_monitor_info(module);
		BroadcastDesktopConfiguration(module);
	}
out:
	module_list_itr_finish(&moditr);
	/* Reissue the DesktopSize command here, rather than sending
	 * down the DesktopSize -- we want FvwmPager in particular to
	 * react to a M_NEW_PAGE event, which DesktopSize will do; and
	 * this avoids duplication in FvwmPager as a result.
	 */

	/* Every monitor will have the same dx/dy values, so just take
	 * the fist entry in our list.
	 */
	m = RB_MIN(monitors, &monitor_q);

	snprintf(action, sizeof(action), "DesktopSize %dx%d", m->dx, m->dy);
	execute_function_override_window(NULL, NULL, action, NULL, 0, NULL);
}

void BroadcastWindowIconNames(FvwmWindow *fw, Bool window, Bool icon)
{
	if (window)
	{
		BroadcastName(
			M_WINDOW_NAME, FW_W(fw), FW_W_FRAME(fw),
			(unsigned long)fw, fw->name.name);
		BroadcastName(
			M_VISIBLE_NAME, FW_W(fw), FW_W_FRAME(fw),
			(unsigned long)fw, fw->visible_name);
	}
	if (icon)
	{
		BroadcastName(
			M_ICON_NAME, FW_W(fw), FW_W_FRAME(fw),
			(unsigned long)fw, fw->icon_name.name);
		BroadcastName(
			MX_VISIBLE_ICON_NAME, FW_W(fw), FW_W_FRAME(fw),
			(unsigned long)fw, fw->visible_icon_name);
	}

	return;
}

/* Capture borrowed picture metadata before any module callback runs. */
static int make_picture_packet(unsigned long *body, unsigned long event_type,
	unsigned long data1, unsigned long data2, unsigned long data3,
	const FvwmPicture *picture, const char *name)
{
	return make_named_packet(body, event_type, name, 9, data1, data2, data3,
		picture == NULL ? 0UL : picture->width,
		picture == NULL ? 0UL : picture->height,
		picture == NULL ? 0UL : picture->depth,
		picture == NULL ? 0UL : picture->picture,
		picture == NULL ? 0UL : picture->mask,
		picture == NULL ? 0UL : picture->alpha);
}

void SendFvwmPicture(
	fmodule *module, unsigned long event_type, unsigned long data1,
	unsigned long data2, unsigned long data3, FvwmPicture *picture,
	char *name)
{
	unsigned long body[FvwmPacketMaxSize];
	int l;

	if (!FMiniIconsSupported)
	{
		return;
	}
	if ((name == NULL) || (event_type != M_MINI_ICON))
	{
		return;
	}

	l = make_picture_packet(body, event_type,
		data1, data2, data3, picture, name);
	PositiveWrite(module, body, l*sizeof(unsigned long));

	return;
}

void BroadcastFvwmPicture(
	unsigned long event_type, unsigned long data1, unsigned long data2,
	unsigned long data3, FvwmPicture *picture, char *name)
{
	unsigned long body[FvwmPacketMaxSize];
	int l;
	fmodule_list_itr moditr;
	fmodule *module;

	if (!FMiniIconsSupported || name == NULL)
	{
		return;
	}
	l = make_picture_packet(body, event_type,
		data1, data2, data3, picture, name);
	module_list_itr_init(&moditr);
	while ( (module = module_list_itr_next(&moditr)) != NULL)
	{
		PositiveWrite(module, body, l*sizeof(unsigned long));
	}

	return;
}

/*
 * Reads a colorset command from a module and broadcasts it back out
 */
void BroadcastColorset(int n)
{
	fmodule_list_itr moditr;
	fmodule *module;
	char *buf;

	buf = DumpColorset(n, GetColorset(n));
	module_list_itr_init(&moditr);
	while ( (module = module_list_itr_next(&moditr)) != NULL)
	{
		SendName(module, M_CONFIG_INFO, 0, 0, 0, buf);
	}

	return;
}

/*
 * Broadcasts a string to all modules as M_CONFIG_INFO.
 */
void BroadcastPropertyChange(
	unsigned long argument, unsigned long data1, unsigned long data2,
	char *string)
{
	fmodule_list_itr moditr;
	fmodule *module;

	module_list_itr_init(&moditr);
	while ( (module = module_list_itr_next(&moditr)) != NULL)
	{
		SendName(module, MX_PROPERTY_CHANGE, argument,
			 data1, data2, string);
	}

	return;
}

/*
 * Broadcasts a string to all modules as M_CONFIG_INFO.
 */
void BroadcastConfigInfoString(char *string)
{
	fmodule_list_itr moditr;
	fmodule *module;

	module_list_itr_init(&moditr);
	while ( (module = module_list_itr_next(&moditr)) != NULL)
	{
		SendName(module, M_CONFIG_INFO, 0, 0, 0, string);
	}

	return;
}

/*
 * Broadcasts the ignored modifiers to all modules as M_CONFIG_INFO.
 */
void broadcast_ignore_modifiers(void)
{
	char msg[32];

	snprintf(msg, sizeof(msg), "IgnoreModifiers %d", GetUnusedModifiers());
	BroadcastConfigInfoString(msg);

	return;
}

/* run the input command as if it cames from a button press or release */
void module_input_execute(struct fmodule_input *input)
{
	XEvent e;
	const exec_context_t *exc;
	exec_context_changes_t ecc;
	int flags;

	/* A nested wait can receive more input; release capacity before dispatch. */
	module_input_dequeue(input);
	input->module->xactiveInputs++;
	memset(&e, 0, sizeof(e));
	if (XFindContext(dpy, input->window, FvwmContext,
				 (caddr_t *)&ecc.w.fw) == XCNOENT)
	{
		ecc.w.fw = NULL;
		input->window = None;
	}
	/* Query the pointer, the pager-drag-out feature doesn't work properly.
	 * This is OK now that the Pager uses "Move pointer"
	 * A real fix would be for the modules to pass the button press coords
	 */
	FQueryPointer(
		    dpy, Scr.Root, &JunkRoot, &JunkChild, &JunkX,&JunkY,
		    &e.xbutton.x_root, &e.xbutton.y_root, &e.xbutton.state);
	e.xbutton.window = input->window;
	e.xbutton.subwindow = None;
	e.xbutton.button = 1;
	/* If a module does XUngrabPointer(), it can now get proper Popups */
	if (StrEquals(input->command, "popup"))
	{
		e.xbutton.type = ButtonPress;
		e.xbutton.state |= Button1Mask;
	}
	else
	{
		e.xbutton.type = ButtonRelease;
		e.xbutton.state &= (~(Button1Mask));
	}
	e.xbutton.x = 0;
	e.xbutton.y = 0;
	fev_fake_event(&e);
	ecc.type = EXCT_MODULE;
	ecc.w.w = input->window;
	flags = (input->window == None) ? 0 : FUNC_DONT_DEFER;
	ecc.w.wcontext = GetContext(NULL, ecc.w.fw, &e, &(input->window));
	ecc.x.etrigger = &e;
	ecc.m.module = input->module;
	exc = exc_create_context(
		&ecc, ECC_TYPE | ECC_ETRIGGER | ECC_FW | ECC_W | ECC_WCONTEXT |
		ECC_MODULE);
	execute_function(NULL, exc, input->command, NULL, flags);
	exc_destroy_context(exc);
	input->module->xactiveInputs--;
	module_input_discard(input);

	return;
}

/* enqueue a module command on the command queue to be executed later  */
void module_input_enqueue(struct fmodule_input *input)
{
	if (input == NULL)
	{
		return;
	}

	module_command_queue_add(&cqueue, input);
}

/*
 *
 *  Procedure:
 *      ExecuteCommandQueue - runs command from the module command queue
 *      This may be called recursively if a module command runs a function
 *      that does a Wait, so it must be re-entrant
 *
 */

static void execute_queued_module_input(void *object, void *context)
{
	fmodule_input *input = object;

	if (input->command)
	{
		module_input_execute(input);
	}
	else
	{
		module_input_discard(input);
	}
}

Bool ExecuteCommandQueue(void)
{
	return module_command_queue_drain(
		&cqueue, &cqueue_drain_state, FVWM_MODULE_COMMAND_BUDGET,
		execute_queued_module_input, NULL);
}

static Bool interaction_message_async(
	const fmodule *module, unsigned long message)
{
	return !IS_MESSAGE_SELECTED(module, message) ||
		!IS_MESSAGE_IN_MASK(&module->xSyncMask, message);
}

Bool interaction_command_ready(const char *command, fmodule *origin)
{
	const func_t *builtin = functions_interaction_builtin(command);
	fmodule_list_itr iterator;
	fmodule *module;
	unsigned long message;

	if (builtin == NULL)
	{
		return False;
	}
	switch (builtin->func_c)
	{
	case F_ECHO:
		message = MX_ECHO;
		break;
	case F_SEND_STRING:
		/* Conservatively defer if any receiver uses synchronous strings;
		 * the target pattern may itself be expanded by the engine. */
		message = M_STRING;
		break;
	case F_SET_MASK:
		/* Enabling a subscription to an existing synchronous frame mask
		 * could introduce an arbitrary-command wait in the drag itself. */
		return origin == NULL ||
			(origin->xSyncMask.m1 == 0 && origin->xSyncMask.m2 == 0);
	case F_SEND_REPLY:
		return origin == NULL ||
			interaction_message_async(origin, MX_REPLY);
	default:
		return True;
	}
	module_list_itr_init(&iterator);
	while ((module = module_list_itr_next(&iterator)) != NULL)
	{
		if (!interaction_message_async(module, message))
		{
			module_list_itr_finish(&iterator);
			return False;
		}
	}
	return True;
}

static Bool interaction_input_ready(const fmodule_input *input)
{
	return interaction_command_ready(input->command, input->module);
}

Bool ExecuteInteractionCommandQueue(void)
{
	unsigned int scans = FVWM_MODULE_COMMAND_BUDGET;
	unsigned int commands = FVWM_INTERACTION_COMMAND_BUDGET;
	fmodule_input *input;
	void *saved_event;

	if (cqueue_drain_state.depth > 0 &&
	    commands > cqueue_drain_state.remaining)
	{
		commands = cqueue_drain_state.remaining;
	}
	saved_event = fev_save_event();
	while (commands > 0 && !isTerminated &&
	       (input = module_command_queue_take_ready(
		&cqueue, &scans, interaction_input_ready)) != NULL)
	{
		commands--;
		if (cqueue_drain_state.depth > 0)
		{
			cqueue_drain_state.remaining--;
		}
		module_input_execute(input);
	}
	fev_restore_event(saved_event);
	/* Yield to I/O, then resume an unfinished bounded scan immediately.
	 * A completed all-deferred pass uses the normal finite wait. */
	return cqueue.cursor != NULL;
}

void ModuleCommandQueueAfterPoll(void)
{
	fvwm_queue_drain_after_poll(
		&cqueue_drain_state, FVWM_MODULE_COMMAND_BUDGET);
}

/*
** send an arbitrary string to all instances of a module
*/
void CMD_SendToModule(F_CMD_ARGS)
{
	char *name,*str;
	unsigned long data0, data1, data2;
	fmodule_list_itr moditr;
	fmodule *module;
	FvwmWindow * const fw = exc->w.fw;

	/* FIXME: Without this, popup menus can't be implemented properly in
	 *  modules.  Olivier: Why ? */
	/* UngrabEm(); */
	if (!action)
	{
		return;
	}
	str = GetNextToken(action, &name);
	if (!name)
	{
		return;
	}

	if (fw)
	{
		/* Modules may need to know which window this applies to */
		data0 = FW_W(fw);
		data1 = FW_W_FRAME(fw);
		data2 = (unsigned long)fw;
	}
	else
	{
		data0 = 0;
		data1 = 0;
		data2 = 0;
	}

	module_list_itr_init(&moditr);
	while ( (module = module_list_itr_next(&moditr)) != NULL)
	{
		if (
			(MOD_NAME(module) != NULL &&
			 matchWildcards(name,MOD_NAME(module))) ||
			(MOD_ALIAS(module) &&
			 matchWildcards(name, MOD_ALIAS(module))))
		{
			SendName(module,M_STRING,data0,data1,data2,str);
			FlushMessageQueue(module);
		}
	}

	free(name);

	return;
}

/*
** send an arbitrary string back to the calling module
*/
void CMD_Send_Reply(F_CMD_ARGS)
{
	unsigned long data0, data1, data2;
	fmodule *module = exc->m.module;
	FvwmWindow * const fw = exc->w.fw;

	if (module == NULL)
	{
		return;
	}

	if (!action)
	{
		return;
	}

	if (fw)
	{
		/* Modules may need to know which window this applies to */
		data0 = FW_W(fw);
		data1 = FW_W_FRAME(fw);
		data2 = (unsigned long)fw;
	}
	else
	{
		data0 = 0;
		data1 = 0;
		data2 = 0;
	}
	SendName(module, MX_REPLY, data0, data1, data2, action);
	FlushMessageQueue(module);

	return;
}

/* A bounded read of core-owned state; no X round trip or retained cursor.
 * Existing directed packets/queues preserve producer command ordering. */
void CMD_Send_Query(F_CMD_ARGS)
{
	struct fvwm_query_spec spec;
	fmodule *module = exc->m.module;
	cJSON *result = NULL, *envelope;
	const char *error = NULL;
	char *json;
	char line[2 * MODULE_QUERY_CHUNK + 160];
	char generation[MODULE_QUERY_GENERATION_LEN + 1];
	unsigned int part = 0;
	size_t length;
	static const char hex[] = "0123456789abcdef";

	if (!module || !fvwm_query_parse(action, &spec))
		return;
	if (strcmp(spec.resource, "topology") == 0)
		result = fvwm_query_topology(
			FScreenGetTopologySnapshot(), &error);
	else if (strcmp(spec.resource, "workspaces") == 0)
		result = fvwm_query_workspaces(&monitor_q,
			monitor_get_all_widths(), monitor_get_all_heights(),
			monitor_mode == MONITOR_TRACKING_M ? "per-monitor" :
			is_tracking_shared ? "shared" : "global", &error);
	else if (strcmp(spec.resource, "bindings") == 0)
	{
		if (binding_query_token(generation))
			result = fvwm_query_bindings(Scr.AllBindings,
				generation, &spec, &error);
		else
			error = "generation_unavailable";
	}
	else
		error = "unsupported_resource";
	envelope = cJSON_CreateObject();
	if (!envelope)
	{
		cJSON_Delete(result);
		return;
	}
	if (result)
	{
		if (!cJSON_AddItemToObject(envelope, "result", result))
		{
			cJSON_Delete(result);
			cJSON_Delete(envelope);
			return;
		}
	}
	else
	{
		cJSON *details = cJSON_AddObjectToObject(envelope, "error");

		if (!details || !cJSON_AddStringToObject(details, "code",
			error ? error : "allocation_failed"))
		{
			cJSON_Delete(envelope);
			return;
		}
	}
	json = cJSON_PrintUnformatted(envelope);
	cJSON_Delete(envelope);
	if (!json)
		return;
	length = strlen(json);
	if (length > MODULE_QUERY_RESULT_MAX)
	{
		cJSON_free(json);
		json = NULL;
		/* Refuse the whole snapshot rather than returning partial data. */
		const char *failure =
			"{\"error\":{\"code\":\"query_too_large\"}}";

		length = strlen(failure);
		json = cJSON_malloc(length + 1);
		if (!json)
			return;
		memcpy(json, failure, length + 1);
	}
	for (size_t offset = 0; offset < length; part++)
	{
		size_t bytes = length - offset;
		int prefix;

		if (bytes > MODULE_QUERY_CHUNK)
			bytes = MODULE_QUERY_CHUNK;
		prefix = snprintf(line, sizeof(line),
			"MFL_CORE_1 %s %s part %u ", spec.cookie,
			spec.serial, part);
		for (size_t i = 0; i < bytes; i++)
		{
			unsigned char value = json[offset + i];

			line[prefix + 2 * i] = hex[value >> 4];
			line[prefix + 2 * i + 1] = hex[value & 15];
		}
		line[prefix + 2 * bytes] = '\0';
		SendName(module, MX_REPLY, 0, 0, 0, line);
		offset += bytes;
	}
	cJSON_free(json);
	snprintf(line, sizeof(line), "MFL_CORE_1 %s %s end %u",
		spec.cookie, spec.serial, part);
	SendName(module, MX_REPLY, 0, 0, 0, line);
	FlushMessageQueue(module);
}

void CMD_Send_WindowList(F_CMD_ARGS)
{
	FvwmWindow *t;
	fmodule *mod = exc->m.module;
	struct monitor	*m;

	if (mod == NULL)
	{
		return;
	}

	/* TA: 2020-01-09:  We send this for *ALL* configured monitors.
	 *
	 * Modules can filter out those monitor packets they're not interested
	 * in.
	 */
	RB_FOREACH(m, monitors, &monitor_q) {
		SendPacket(mod, M_NEW_DESK, 2, (long)m->virtual_scr.CurrentDesk,
			(long)m->si->rr_output);
		SendPacket(
			mod, M_NEW_PAGE, 8, (long)m->virtual_scr.Vx, (long)m->virtual_scr.Vy,
			(long)m->virtual_scr.CurrentDesk,
			(long) monitor_get_all_widths(),
			(long) monitor_get_all_heights(),
			(long)((m->virtual_scr.VxMax / monitor_get_all_widths()) + 1),
			(long)((m->virtual_scr.VyMax / monitor_get_all_heights()) + 1),
			(long)m->si->rr_output);

		if (Scr.Hilite != NULL)
		{
			SendPacket(
				mod, M_FOCUS_CHANGE, 5, (long)FW_W(Scr.Hilite),
				(long)FW_W_FRAME(Scr.Hilite), (unsigned long)True,
				(long)Scr.Hilite->hicolors.fore,
				(long)Scr.Hilite->hicolors.back);
		}
		else
		{
			SendPacket(
				mod, M_FOCUS_CHANGE, 5, 0, 0, (unsigned long)True,
				(long)GetColorset(0)->fg, (long)GetColorset(0)->bg);
		}
		if (Scr.DefaultIcon != NULL)
		{
			SendName(mod, M_DEFAULTICON, 0, 0, 0, Scr.DefaultIcon);
		}

		for (t = Scr.FvwmRoot.next; t != NULL; t = t->next)
		{
			if ((monitor_mode == MONITOR_TRACKING_M) && t->m != m)
				continue;

			SendConfig(mod,M_CONFIGURE_WINDOW,t);
			SendName(
				mod, M_WINDOW_NAME, FW_W(t), FW_W_FRAME(t),
				(unsigned long)t, t->name.name);
			SendName(
				mod, M_ICON_NAME, FW_W(t), FW_W_FRAME(t),
				(unsigned long)t, t->icon_name.name);
			SendName(
				mod, M_VISIBLE_NAME, FW_W(t), FW_W_FRAME(t),
				(unsigned long)t, t->visible_name);
			SendName(
				mod, MX_VISIBLE_ICON_NAME, FW_W(t), FW_W_FRAME(t),
				(unsigned long)t,t->visible_icon_name);
			if (t->icon_bitmap_file != NULL
			    && t->icon_bitmap_file != Scr.DefaultIcon)
			{
				SendName(
					mod, M_ICON_FILE, FW_W(t), FW_W_FRAME(t),
					(unsigned long)t, t->icon_bitmap_file);
			}

			SendName(
				mod, M_RES_CLASS, FW_W(t), FW_W_FRAME(t),
				(unsigned long)t, t->class.res_class);
			SendName(
				mod, M_RES_NAME, FW_W(t), FW_W_FRAME(t),
				(unsigned long)t, t->class.res_name);

			if (IS_ICONIFIED(t) && !IS_ICON_UNMAPPED(t))
			{
				rectangle r;
				Bool rc;

				rc = get_visible_icon_geometry(t, &r);
				if (rc == True)
				{
					SendPacket(
						mod, M_ICONIFY, 7, (long)FW_W(t),
						(long)FW_W_FRAME(t), (unsigned long)t,
						(long)r.x, (long)r.y,
						(long)r.width, (long)r.height);
				}
			}
			if ((IS_ICONIFIED(t))&&(IS_ICON_UNMAPPED(t)))
			{
				SendPacket(
					mod, M_ICONIFY, 7, (long)FW_W(t),
					(long)FW_W_FRAME(t), (unsigned long)t,
					(long)0, (long)0, (long)0, (long)0);
			}
			if (FMiniIconsSupported && t->mini_icon != NULL)
			{
				SendFvwmPicture(
					mod, M_MINI_ICON, FW_W(t), FW_W_FRAME(t),
					(unsigned long)t, t->mini_icon,
					t->mini_pixmap_file);
			}
		}

		if (Scr.Hilite == NULL)
		{
			BroadcastPacket(
				M_FOCUS_CHANGE, 5, (long)0, (long)0,
				(unsigned long)True, (long)GetColorset(0)->fg,
				(long)GetColorset(0)->bg);
		}
		else
		{
			BroadcastPacket(
				M_FOCUS_CHANGE, 5, (long)FW_W(Scr.Hilite),
				(long)FW_W(Scr.Hilite), (unsigned long)True,
				(long)Scr.Hilite->hicolors.fore,
				(long)Scr.Hilite->hicolors.back);
		}

		/* If we're tracking just the global monitor, then stop here
		 * -- essentially, we only need to send this information once.
		 */
		if (monitor_mode == MONITOR_TRACKING_G)
			break;
	}

	SendPacket(mod, M_END_WINDOWLIST, 0);
}
