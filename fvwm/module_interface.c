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
#include "libs/module_ui_scale.h"
#include "libs/wild.h"
#include "libs/FEvent.h"
#include "fvwm.h"
#include "externs.h"

#include "functions.h"
#include "bindings.h"
#include "menubindings.h"
#include "move_resize.h"
#include "misc.h"
#include "screen.h"
#include "module_interface.h"
#include "module_list.h"
#include "monitor_scale_config.h"
#include "event_loop_policy.h"
#include "events.h"
#include "geometry.h"
#include "libs/fvwmsignal.h"
#include "decorations.h"
#include "ewmh.h"
#include "commands.h"
#include "module_queue.h"
#include "query_json.h"
#include "appearance_preview.h"
#include "root_background.h"
#include "startup_notification.h"
#include "session_clients.h"
#include "action_palette.h"
#include "workspace_tools.h"
#include "workspace_dock.h"
#include "infostore.h"
#include "libs/json_print_policy.h"
#include <limits.h>
#include "colorset.h"
#include "menustyle.h"
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

void send_config_source(fmodule *module, unsigned long key,
	const char *source, const char *config)
{
	unsigned long source_body[FvwmPacketMaxSize];
	unsigned long config_body[FvwmPacketMaxSize];
	int source_length, config_length;

	/* Copy borrowed list data before synchronous module callbacks can remove
	 * its entry. The new metadata must not introduce an intervening callback. */
	source_length = make_named_packet(source_body, MX_CONFIG_SOURCE,
		source, 3, key, 0UL, 0UL);
	config_length = make_named_packet(config_body, M_CONFIG_INFO,
		config, 3, key, 0UL, 0UL);
	module_output_enqueue(module, source_body,
		source_length * sizeof(unsigned long));
	PositiveWrite(module, config_body,
		config_length * sizeof(unsigned long));
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

/* Copy authoritative facts and effective policy before the first synchronous
 * module callback can replace topology or retire a runtime monitor. */
static module_ui_scale_bundle *prepare_monitor_ui_scale(void)
{
	const monitor_topology_snapshot *topology = FScreenGetTopologySnapshot();
	module_ui_scale_snapshot *snapshot;
	module_ui_scale_bundle *bundle;

	if (topology == NULL || topology->epoch == 0 ||
	    topology->monitor_count < 1 ||
	    topology->monitor_count > MODULE_UI_SCALE_MAX ||
	    topology->monitors == NULL ||
	    !monitor_scale_config_is_finalized(monitor_scale_config_get()))
		return NULL;
	snapshot = fxcalloc(1, sizeof(*snapshot));
	snapshot->epoch = topology->epoch;
	snapshot->count = topology->monitor_count;
	snapshot->root_fallback = topology->root_fallback;
	for (unsigned int i = 0; i < snapshot->count; i++)
	{
		const monitor_topology_monitor *fact = &topology->monitors[i];
		module_ui_scale_row *row = &snapshot->rows[i];
		struct monitor *runtime = NULL, *m;
		size_t length;

		if (fact->name == NULL)
			goto invalid;
		length = strnlen(fact->name, MODULE_UI_SCALE_NAME_MAX + 1);
		if (length == 0 || length > MODULE_UI_SCALE_NAME_MAX)
			goto invalid;
		RB_FOREACH(m, monitors, &monitor_q)
		{
			if (m->si == NULL || m->si->name == NULL ||
			    strcmp(m->si->name, fact->name) != 0 ||
			    (m->si->name_atom != None &&
			     m->si->name_atom != fact->name_atom))
				continue;
			if (runtime != NULL)
				goto invalid;
			runtime = m;
		}
		if (runtime == NULL)
			goto invalid;
		row->name_atom = fact->name_atom;
		row->quarters = runtime->effective_scale.factor.quarters;
		row->primary = fact->primary;
		row->rect = (rectangle){fact->x, fact->y,
			fact->width, fact->height};
		memcpy(row->name, fact->name, length + 1);
	}
	bundle = fxmalloc(sizeof(*bundle));
	if (!module_ui_scale_format(snapshot, bundle))
	{
		free(bundle);
		goto invalid;
	}
	free(snapshot);
	return bundle;
invalid:
	free(snapshot);
	return NULL;
}

static void send_monitor_ui_scale(fmodule *module,
	const module_ui_scale_bundle *bundle)
{
	unsigned long packet[FvwmPacketMaxSize];

	_Static_assert(MODULE_UI_SCALE_LINE_MAX <=
		(FvwmPacketBodyMaxSize - 3) * sizeof(unsigned long),
		"MonitorUI records must fit the existing native packet");
	if (bundle == NULL)
		return;
	/* Keep each existing M_CONFIG_INFO synchronization barrier. A nested
	 * refresh may interleave complete copied transactions; the consumer stages
	 * and validates their epochs before replacing its last good publication. */
	for (unsigned int i = 0; i < bundle->count; i++)
	{
		int length = make_named_packet(packet, M_CONFIG_INFO,
			bundle->lines[i], 3, 0UL, 0UL, 0UL);

		PositiveWrite(module, packet, length * sizeof(*packet));
	}
}

void broadcast_monitor_ui_scale(void)
{
	module_ui_scale_bundle *bundle = prepare_monitor_ui_scale();
	fmodule_list_itr iterator;
	fmodule *module;

	if (bundle == NULL)
	{
		fvwm_debug(__func__, "MonitorUI unavailable; retaining module scale");
		return;
	}
	module_list_itr_init(&iterator);
	while ((module = module_list_itr_next(&iterator)) != NULL)
		send_monitor_ui_scale(module, bundle);
	module_list_itr_finish(&iterator);
	free(bundle);
}

void BroadcastMonitorList(fmodule *this)
{
	char action[256];
	struct monitor	*m;
	fmodule_list_itr moditr;
	fmodule *module;
	module_ui_scale_bundle *ui_scale = prepare_monitor_ui_scale();

	if (ui_scale == NULL)
		fvwm_debug(__func__, "MonitorUI unavailable; retaining module scale");

	module_list_itr_init(&moditr);

	if (this != NULL) {
		/*
		 * We've been requested to send this information to a specific
		 * module only.
		 */
		send_monitor_ui_scale(this, ui_scale);
		send_monitor_info(this);
		BroadcastDesktopConfiguration(this);
		goto out;
	}

	while ((module = module_list_itr_next(&moditr)) != NULL) {
		send_monitor_ui_scale(module, ui_scale);
		send_monitor_info(module);
		BroadcastDesktopConfiguration(module);
	}
out:
	module_list_itr_finish(&moditr);
	free(ui_scale);
	/* Reissue the DesktopSize command here, rather than sending
	 * down the DesktopSize -- we want FvwmPager in particular to
	 * react to a M_NEW_PAGE event, which DesktopSize will do; and
	 * this avoids duplication in FvwmPager as a result.
	 */

	/* Every monitor will have the same dx/dy values, so just take
	 * the fist entry in our list.
	 */
	m = RB_MIN(monitors, &monitor_q);
	if (m == NULL)
		return;

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
	case F_SEND_PREVIEW:
	case F_SEND_QUERY:
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

static cJSON *query_colorsets(const struct fvwm_query_spec *spec,
	const char **error)
{
	struct fvwm_query_colorset entries[MODULE_QUERY_PAGE_MAX];
	char generation[MODULE_QUERY_GENERATION_LEN + 1];
	size_t total = ColorsetCount(), count = 0;
	bool single = strcmp(spec->resource, "colorset") == 0;
	bool summary = strcmp(spec->resource, "color-summary") == 0;

	*error = "generation_unavailable";
	if (!colorset_query_token(generation))
		return NULL;
	/* Validate freshness before looking up a cursor in a changed table. */
	if ((strcmp(spec->generation, "-") != 0 &&
	     strcmp(spec->generation, generation) != 0) ||
	    (!single && spec->offset && strcmp(spec->generation, "-") == 0))
	{
		*error = "stale_generation";
		return NULL;
	}
	if (single)
	{
		const colorset_t *cs = FindColorset((int)spec->offset);

		*error = "unavailable";
		if (!cs)
			return NULL;
		entries[count++] = (struct fvwm_query_colorset){
			(int)spec->offset, cs,
			colorset_query_update_state((int)spec->offset)};
	}
	else if (!summary)
	{
		*error = "invalid_offset";
		if (spec->offset > total)
			return NULL;
		for (size_t i = spec->offset;
		     i < total && count < spec->limit; i++)
		{
			int id = ColorsetIdAt(i);

			entries[count++] = (struct fvwm_query_colorset){id,
				FindColorset(id),
				colorset_query_update_state(id)};
		}
	}
	return fvwm_query_colorsets(entries, count, total, generation,
		spec, error);
}

/* Consume result on every path. Bounded directed packets preserve producer
 * command ordering for both reads and owned preview actions. */
static void send_query_result(fmodule *module, const char *cookie,
	const char *serial, cJSON *result, const char *error)
{
	cJSON *envelope;
	char *json;
	char line[2 * MODULE_QUERY_CHUNK + 160];
	unsigned int part = 0;
	size_t length;
	static const char hex[] = "0123456789abcdef";

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
			"MFL_CORE_1 %s %s part %u ", cookie,
			serial, part);
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
		cookie, serial, part);
	SendName(module, MX_REPLY, 0, 0, 0, line);
	FlushMessageQueue(module);
}

void CMD_Send_Query(F_CMD_ARGS)
{
	struct fvwm_query_spec spec;
	fmodule *module = exc->m.module;
	cJSON *result = NULL;
	const char *error = NULL;
	char generation[MODULE_QUERY_GENERATION_LEN + 1];

	if (!module || !fvwm_query_parse(action, &spec))
		return;
	if (strcmp(spec.resource, "appearance") == 0 ||
	    strcmp(spec.resource, "menu-styles") == 0 ||
	    strcmp(spec.resource, "decorations") == 0)
	{
		if (!fvwm_query_appearance_token(generation))
			error = "generation_unavailable";
		else if (strcmp(spec.resource, "appearance") == 0)
		{
			result = fvwm_query_appearance(generation,
				&spec, &error);
			if (result)
			{
				cJSON *capability = appearance_preview_capability();

				if (!capability || !cJSON_AddItemToObject(result,
				    "resource_preview", capability))
				{
					cJSON_Delete(capability);
					cJSON_Delete(result);
					result = NULL;
					error = "allocation_failed";
				}
			}
			if (result)
			{
				cJSON *capability =
					appearance_opaque_colors_capability();

				if (!capability ||
				    !cJSON_AddItemToObject(result,
				    "opaque_colors_preview", capability))
				{
					cJSON_Delete(capability);
					cJSON_Delete(result);
					result = NULL;
					error = "allocation_failed";
				}
			}
			if (result)
			{
				cJSON *capability = appearance_complete_bundle_capability();

				if (!capability || !cJSON_AddItemToObject(result,
					"complete_bundle_preview", capability))
				{
					cJSON_Delete(capability);
					cJSON_Delete(result);
					result = NULL;
					error = "allocation_failed";
				}
			}
		}
		else if (strcmp(spec.resource, "menu-styles") == 0)
			result = fvwm_query_menu_styles(
				menustyle_get_default_style(), generation,
				&spec, &error);
		else
			result = fvwm_query_decorations(&Scr.DefaultDecor,
				generation, &spec, &error);
	}
	else if (strcmp(spec.resource, "colorsets") == 0 ||
	    strcmp(spec.resource, "colorset") == 0 ||
	    strcmp(spec.resource, "color-summary") == 0)
		result = query_colorsets(&spec, &error);
	else if (strcmp(spec.resource, "topology") == 0)
		result = fvwm_query_topology(
			FScreenGetTopologySnapshot(), &error);
	else if (strcmp(spec.resource, "startups") == 0)
		result = startup_notification_query(&spec, &error);
	else if (strcmp(spec.resource, "session-clients") == 0)
		result = session_clients_query(&spec, &error);
	else if (strcmp(spec.resource, "palette") == 0)
		result = action_palette_query(module, &spec, &error);
	else if (strcmp(spec.resource, "workspaces") == 0)
		result = fvwm_query_workspaces(&monitor_q,
			monitor_get_all_widths(), monitor_get_all_heights(),
			monitor_mode == MONITOR_TRACKING_M ? "per-monitor" :
			is_tracking_shared ? "shared" : "global", &error);
	else if (strcmp(spec.resource, "bindings") == 0 ||
	    strcmp(spec.resource, "menu-bindings") == 0 ||
	    strcmp(spec.resource, "menu-defaults") == 0 ||
	    strcmp(spec.resource, "placement") == 0 ||
	    strcmp(spec.resource, "binding-policy") == 0)
	{
		if (binding_query_token(generation))
		{
			if (strcmp(spec.resource, "placement") == 0)
			{
				unsigned int drag, interactive;
				const fvwm_policy_origin *drag_origin;
				const fvwm_policy_origin *interactive_origin;

				placement_query_origins(&drag_origin,
					&interactive_origin);
				placement_query_masks(&drag, &interactive);
				result = fvwm_query_placement(generation, &spec,
					drag, interactive, drag_origin,
					interactive_origin, &error);
			}
			else if (strcmp(spec.resource, "binding-policy") == 0)
				result = fvwm_query_binding_policy(generation,
					&spec, GetUnusedModifiers(),
					binding_query_ignored_origin(), &error);
			else
				result = fvwm_query_bindings(
					strcmp(spec.resource, "bindings") == 0 ?
					Scr.AllBindings : menu_query_bindings(
						strcmp(spec.resource,
							"menu-defaults") == 0),
					generation, &spec, &error);
		}
		else
			error = "generation_unavailable";
	}
	else
		error = "unsupported_resource";
	send_query_result(module, spec.cookie, spec.serial, result, error);
}

void CMD_Send_Root(F_CMD_ARGS)
{
	/* Fixed internal selector: no pathname, command or public pixmap. */
	if (exc->m.module && action && strcmp(action, "1 20") == 0)
		root_background_select();
}

void CMD_Send_Preview(F_CMD_ARGS)
{
	struct preview_request request;
	const char *error = NULL;
	cJSON *result;
	fmodule *module = exc->m.module;

	if (!module || !preview_request_parse(action, &request))
		return;
	result = appearance_preview_request(module, &request, &error);
	send_query_result(module, request.cookie, request.serial, result, error);
}

void CMD_Send_Session(F_CMD_ARGS)
{
	struct session_clients_request request;
	fmodule *module = exc->m.module;
	const char *error = NULL;
	char command[96];
	cJSON *result;

	if (!module || !session_clients_parse(action, &request))
		return;
	result = session_clients_action(&request, command, &error);
	if (result && command[0] &&
	    request.operation == SESSION_CLIENTS_QUIT_IF_EMPTY)
	{
		/* Quit may never return. Publish only a request acknowledgment,
		 * then repeat the final guard after output handling. No
		 * dispatch/flush/callback between that guard and the engine. */
		send_query_result(module, request.cookie, request.serial,
			result, NULL);
		if (!MOD_IS_RETIRED(module) &&
		    session_clients_quit_ready(request.generation))
			execute_function_override_window(NULL, exc,
				command, NULL,
				FUNC_DONT_EXPAND_COMMAND, NULL);
		return;
	}
	if (result && command[0])
	{
		/* The helper copied a fixed command after the identity check.
		 * No borrowed window survives this recursive engine call. */
		execute_function_override_window(NULL, exc, command, NULL,
			FUNC_DONT_EXPAND_COMMAND, NULL);
		if (MOD_IS_RETIRED(module))
		{
			cJSON_Delete(result);
			return;
		}
	}
	send_query_result(module, request.cookie, request.serial,
		result, error);
}

void CMD_Send_Palette(F_CMD_ARGS)
{
	struct palette_request request;
	struct palette_row row;
	char id[PALETTE_ID_MAX + 1], command[128];
	const char *error = NULL;
	cJSON *result = NULL;
	fmodule *module = exc->m.module;

	if (!module || !palette_request_parse(action, &request))
		return;
	if (!action_palette_target(module, &request, &row, id, &error))
		goto reply;
	result = cJSON_CreateObject();
	error = "allocation_failed";
	if (!result ||
	    !cJSON_AddBoolToObject(result, "applied", !request.release &&
		request.operation != PALETTE_DRAG) ||
	    !cJSON_AddStringToObject(result, "action", request.release ?
		"release" : request.operation == PALETTE_DRAG ? "drag" :
		request.operation == PALETTE_MOVE ? "move" :
		row.kind == PALETTE_WINDOW ? "focus" : "desktop") ||
	    !cJSON_AddStringToObject(result, "target", id))
		goto fail;
	if (!request.release)
	{
		unsigned int steps = request.operation == PALETTE_DRAG ? 4 :
			request.operation == PALETTE_MOVE ? 2 :
			row.kind == PALETTE_WINDOW ? 3 : 1;

		if (request.operation == PALETTE_DRAG &&
		    !cJSON_AddBoolToObject(result, "submitted", true))
			goto fail;
		if (request.operation == PALETTE_MOVE)
		{
			struct palette_row destination = row;
			char destination_id[PALETTE_ID_MAX + 1];

			destination.kind = PALETTE_DESKTOP;
			palette_row_id(destination_id, request.generation,
				&destination);
			if (!cJSON_AddStringToObject(result, "destination",
				destination_id))
				goto fail;
		}

		/* Revalidate allocation identity before each fixed operation.
		 * No borrowed FvwmWindow survives a recursive engine call. */
		for (unsigned int step = 0; step < steps; step++)
		{
			if (!action_palette_command(&row, step, command,
				sizeof(command), &error))
				goto fail;
			execute_function_override_window(NULL, exc, command, NULL,
				FUNC_DONT_EXPAND_COMMAND, NULL);
			if (MOD_IS_RETIRED(module))
			{
				cJSON_Delete(result);
				return;
			}
		}
	}
	error = NULL;
	goto reply;
fail:
	cJSON_Delete(result);
	result = NULL;
reply:
	send_query_result(module, request.cookie, request.serial, result, error);
}

void CMD_Send_Scratchpad(F_CMD_ARGS)
{
	struct scratchpad_request request;
	struct scratchpad_plan plan;
	fmodule *module = exc->m.module;
	cJSON *result;
	const char *error = NULL;
	char command[160];
	unsigned int steps;

	if (!module)
		return;
	if (!scratchpad_request_parse(action, &request))
	{
		char version[3], cookie[65], serial[17], route[90];
		struct palette_request routing;

		/* Reply only when the routing prefix is itself validated. */
		if (action && sscanf(action, "%2s %64s %16s",
		    version, cookie, serial) == 3 && !strcmp(version, "v1"))
		{
			snprintf(route, sizeof(route), "%s %s - -", cookie, serial);
			if (palette_request_parse(route, &routing))
				send_query_result(module, cookie, serial, NULL,
					"invalid-request");
		}
		return;
	}
	result = scratchpad_prepare(module, &request, &plan, &error);
	if (!result)
		goto reply;
	steps = plan.action == SCRATCHPAD_SUMMON ? 5 :
		plan.action == SCRATCHPAD_DISMISS ? 2 : 0;
	for (unsigned int step = 0; step < steps; step++)
	{
		if (!scratchpad_command(&plan, step, command,
			sizeof(command), &error))
		{
			cJSON_Delete(result);
			result = NULL;
			break;
		}
		if (command[0])
			execute_function_override_window(NULL, exc, command, NULL,
				FUNC_DONT_EXPAND_COMMAND, NULL);
		if (MOD_IS_RETIRED(module))
		{
			cJSON_Delete(result);
			return;
		}
	}
reply:
	send_query_result(module, request.cookie, request.serial, result, error);
}

#define DOCK_REQUEST_LEASE_MS 30000

_Static_assert(sizeof("Send_Dock  0000000000000000 record   -65535 -65535") -
	1 + 64 + 2 * WORKSPACE_DOCK_SELECTOR_WIRE_MAX <= MAX_MODULE_INPUT_TEXT_LEN,
	"docking records must fit one native module input frame");

/* A single durable restore transaction pins only copied facts. The module
 * record remains valid until its teardown hook retires the transaction.
 */
static struct {
	fmodule *owner;
	char cookie[65], serial[17];
	size_t expected_count;
	uint64_t panel_revision;
	uint64_t deadline;
	bool panel;
	struct workspace_dock_profile_input input;
	struct workspace_dock_plan plan;
} dock_request;

void workspace_dock_owner_gone(struct fmodule *owner)
{
	if (dock_request.owner == owner)
		memset(&dock_request, 0, sizeof(dock_request));
}

static bool dock_now(uint64_t *milliseconds)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 ||
	    (uint64_t)now.tv_sec > (UINT64_MAX - DOCK_REQUEST_LEASE_MS - 999) / 1000)
		return false;
	*milliseconds = (uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
	return true;
}

static bool dock_serial_valid(const char *serial)
{
	return strlen(serial) == 16 &&
		strspn(serial, "0123456789abcdef") == 16;
}

static bool dock_text_valid(const char *text)
{
	size_t length = strnlen(text, WORKSPACE_DOCKING_NAME_MAX + 1);

	if (!length || length > WORKSPACE_DOCKING_NAME_MAX)
		return false;
	for (const unsigned char *p = (const unsigned char *)text; *p;)
	{
		size_t step = json_utf8_sequence_length(p);

		if (!step || *p < 32 || *p == 127)
			return false;
		p += step;
	}
	return true;
}

static bool dock_decode_hex(const char *hex, char *out)
{
	size_t length = strlen(hex);
	static const char digits[] = "0123456789abcdef";

	if (!length || length > 2 * WORKSPACE_DOCKING_NAME_MAX ||
	    length % 2 || strspn(hex, digits) != length)
		return false;
	for (size_t i = 0; i < length; i += 2)
	{
		unsigned int byte = (strchr(digits, hex[i]) - digits) * 16 +
			(strchr(digits, hex[i + 1]) - digits);

		if (byte < 32 || byte == 127)
			return false;
		out[i / 2] = (char)byte;
	}
	out[length / 2] = '\0';
	return dock_text_valid(out);
}

static bool dock_number(const char *text, int minimum, int maximum, int *out)
{
	const char *digits = *text == '-' && minimum < 0 ? text + 1 : text;
	long value;

	if (!*digits || strspn(digits, "0123456789") != strlen(digits))
		return false;
	errno = 0;
	value = strtol(text, NULL, 10);
	if (errno || value < minimum || value > maximum)
		return false;
	*out = (int)value;
	return true;
}

static bool dock_begin(fmodule *module, char **tokens, size_t count,
	uint64_t now)
{
	char connector[WORKSPACE_DOCKING_NAME_MAX + 1];
	int desk = 0, windows;
	bool has_desk;

	if (count != 7 || !dock_decode_hex(tokens[3], connector) ||
	    !dock_number(tokens[5], 0, WORKSPACE_DOCKING_WINDOW_MAX, &windows) ||
	    (strcmp(tokens[6], "0") != 0 && strcmp(tokens[6], "1") != 0))
		return false;
	has_desk = strcmp(tokens[4], "-") != 0;
	if (has_desk && !dock_number(tokens[4], 0, INT_MAX, &desk))
		return false;
	workspace_dock_owner_gone(module);
	dock_request.owner = module;
	dock_request.deadline = now + DOCK_REQUEST_LEASE_MS;
	strcpy(dock_request.cookie, tokens[0]);
	strcpy(dock_request.serial, tokens[1]);
	dock_request.expected_count = (size_t)windows;
	dock_request.panel = strcmp(tokens[6], "1") == 0;
	strcpy(dock_request.input.connector, connector);
	dock_request.input.has_desk = has_desk;
	dock_request.input.desk = desk;
	return true;
}

static bool dock_record(char **tokens, size_t count)
{
	struct workspace_dock_window_input window = {0};
	int x, y;

	if (count != 7 || dock_request.plan.prepared ||
	    dock_request.input.count >= dock_request.expected_count ||
	    !dock_decode_hex(tokens[3], window.selector.instance) ||
	    !dock_decode_hex(tokens[4], window.selector.class_name) ||
	    strlen(window.selector.instance) + strlen(window.selector.class_name) >
		    WORKSPACE_DOCK_SELECTOR_WIRE_MAX ||
	    !dock_number(tokens[5], -WORKSPACE_DOCK_OFFSET_MAX,
		    WORKSPACE_DOCK_OFFSET_MAX, &x) ||
	    !dock_number(tokens[6], -WORKSPACE_DOCK_OFFSET_MAX,
		    WORKSPACE_DOCK_OFFSET_MAX, &y))
		return false;
	window.offset_x = x;
	window.offset_y = y;
	dock_request.input.windows[dock_request.input.count++] = window;
	return true;
}

static const char *dock_status_error(enum workspace_dock_status status)
{
	switch (status)
	{
	case WORKSPACE_DOCK_INVALID: return "invalid-request";
	case WORKSPACE_DOCK_AMBIGUOUS: return "ambiguous-selector";
	case WORKSPACE_DOCK_STALE: return "stale-plan";
	case WORKSPACE_DOCK_BUSY: return "busy";
	default: return "unavailable";
	}
}

static cJSON *dock_counts(const char *count_name, size_t count, size_t skipped,
	bool fallback)
{
	cJSON *result = cJSON_CreateObject();

	if (!result || !cJSON_AddNumberToObject(result, count_name, count) ||
	    !cJSON_AddNumberToObject(result, "skipped", skipped) ||
	    !cJSON_AddBoolToObject(result, "fallback", fallback))
	{
		cJSON_Delete(result);
		return NULL;
	}
	return result;
}

static const char *dock_monitor_name(const struct workspace_dock_plan *plan)
{
	const monitor_topology_snapshot *snapshot = FScreenGetTopologySnapshot();

	if (!snapshot || snapshot->epoch != plan->destination.epoch)
		return NULL;
	for (int i = 0; i < snapshot->monitor_count; i++)
	{
		const monitor_topology_monitor *monitor = &snapshot->monitors[i];

		if (monitor->name_atom == plan->destination.role && monitor->name &&
		    dock_text_valid(monitor->name))
			return monitor->name;
	}
	return NULL;
}

static cJSON *dock_prepare_reply(const char **error)
{
	const struct workspace_dock_plan *plan = &dock_request.plan;
	const char *monitor;
	char command[WORKSPACE_DOCKING_NAME_MAX + 32];
	cJSON *result;
	enum workspace_dock_status status =
		workspace_dock_prepare(&dock_request.input, &dock_request.plan);

	if (status != WORKSPACE_DOCK_OK)
	{
		*error = dock_status_error(status);
		return NULL;
	}
	if (dock_request.panel)
	{
		dock_request.panel_revision = workspace_panel_revision();
		if (!workspace_panel_command(dock_request.panel_revision,
		    &plan->destination, command, sizeof(command)))
		{
			*error = "unavailable";
			return NULL;
		}
	}
	result = dock_counts("matched", plan->count,
		plan->requested - plan->count, plan->primary_fallback);
	monitor = dock_monitor_name(plan);
	if (!result || (monitor &&
	    !cJSON_AddStringToObject(result, "monitor", monitor)))
	{
		*error = "unavailable";
		cJSON_Delete(result);
		return NULL;
	}
	return result;
}

struct dock_panel_apply {
	fmodule *module;
	const exec_context_t *exc;
	uint64_t revision;
	bool applied;
};

static bool dock_apply_panel(
	const struct workspace_placement_destination *destination, void *context)
{
	struct dock_panel_apply *panel = context;
	char command[WORKSPACE_DOCKING_NAME_MAX + 32];
	char target[sizeof(command)];
	const char *assigned;

	if (MOD_IS_RETIRED(panel->module) ||
	    !workspace_panel_command(panel->revision, destination,
		    command, sizeof(command)))
		return false;
	strcpy(target, command + sizeof("SlatePanelAssign ") - 1);
	execute_function_override_window(NULL, panel->exc, command, NULL,
		FUNC_DONT_EXPAND_COMMAND, NULL);
	assigned = get_metainfo_value(WORKSPACE_PANEL_TARGET);
	panel->applied = assigned && strcmp(assigned, target) == 0;
	return panel->applied && !MOD_IS_RETIRED(panel->module);
}

static cJSON *dock_apply_reply(fmodule *module, const exec_context_t *exc,
	const char **error)
{
	struct workspace_dock_plan plan = dock_request.plan;
	struct workspace_dock_result applied;
	struct dock_panel_apply panel = {module, exc, dock_request.panel_revision,
		false};
	bool apply_panel = dock_request.panel;
	char command[WORKSPACE_DOCKING_NAME_MAX + 32];
	enum workspace_dock_status status;
	cJSON *result;

	/* Retire the lease before any command or output can reenter. */
	workspace_dock_owner_gone(module);
	if (apply_panel &&
	    (panel.revision != workspace_panel_revision() ||
	     !workspace_placement_destination_is_current(&plan.destination)))
	{
		*error = "stale-plan";
		return NULL;
	}
	if (apply_panel && !workspace_panel_command(panel.revision,
	    &plan.destination, command, sizeof(command)))
	{
		*error = "unavailable";
		return NULL;
	}
	status = workspace_dock_apply(&plan, &applied,
		apply_panel ? dock_apply_panel : NULL, &panel);
	if (status != WORKSPACE_DOCK_OK)
	{
		*error = dock_status_error(status);
		return NULL;
	}
	*error = "unavailable";
	result = dock_counts("applied", applied.applied, applied.skipped,
		applied.primary_fallback);
	if (result && !cJSON_AddBoolToObject(result, "panel_applied",
	    panel.applied))
	{
		cJSON_Delete(result);
		result = NULL;
	}
	return result;
}

void CMD_Send_Dock(F_CMD_ARGS)
{
	fmodule *module = exc->m.module;
	char input[MAX_MODULE_INPUT_TEXT_LEN + 1], *tokens[7], *token, *save;
	size_t count = 0;
	uint64_t now;
	const char *error = "invalid-request";
	cJSON *result = NULL;
	bool prepare, apply, owner, transaction;

	if (!module || MOD_IS_RETIRED(module) || !action ||
	    strnlen(action, sizeof(input)) >= sizeof(input))
		return;
	strcpy(input, action);
	for (token = strtok_r(input, " \t\r\n", &save);
	     token && count < sizeof(tokens) / sizeof(tokens[0]);
	     token = strtok_r(NULL, " \t\r\n", &save))
		tokens[count++] = token;
	if (count < 3 || strlen(tokens[0]) > 64 ||
	    strspn(tokens[0], "0123456789abcdef-") != strlen(tokens[0]) ||
	    !dock_serial_valid(tokens[1]))
		return;
	prepare = strcmp(tokens[2], "prepare") == 0;
	apply = strcmp(tokens[2], "apply") == 0;
	if (!dock_now(&now))
	{
		workspace_dock_owner_gone(dock_request.owner);
		if (prepare || apply)
			send_query_result(module, tokens[0], tokens[1], NULL,
				"unavailable");
		return;
	}
	if (dock_request.owner && now >= dock_request.deadline)
		workspace_dock_owner_gone(dock_request.owner);
	owner = dock_request.owner == module &&
		strcmp(tokens[0], dock_request.cookie) == 0;
	transaction = owner && strcmp(tokens[1], dock_request.serial) == 0;

	if (dock_request.owner && dock_request.owner != module)
		error = "busy";
	else if (apply)
	{
		if (!token && count == 4 && dock_serial_valid(tokens[3]) &&
		    strcmp(tokens[1], tokens[3]) != 0)
		{
			error = "stale-plan";
			if (owner && dock_request.plan.prepared &&
			    strcmp(tokens[3], dock_request.serial) == 0)
				result = dock_apply_reply(module, exc, &error);
		}
	}
	else if (prepare)
	{
		if (!token && count == 3 && transaction &&
		    !dock_request.plan.prepared &&
		    dock_request.input.count == dock_request.expected_count)
			result = dock_prepare_reply(&error);
		if (result)
			dock_request.deadline = now + DOCK_REQUEST_LEASE_MS;
		else if (transaction)
			workspace_dock_owner_gone(module);
	}
	else if (!token && strcmp(tokens[2], "begin") == 0 &&
	    dock_begin(module, tokens, count, now))
		return;
	else if (!token && transaction && strcmp(tokens[2], "record") == 0 &&
	    dock_record(tokens, count))
		return;
	else if (transaction)
		workspace_dock_owner_gone(module);

	if ((prepare || apply) && !MOD_IS_RETIRED(module))
		send_query_result(module, tokens[0], tokens[1], result, error);
	else
		cJSON_Delete(result);
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
