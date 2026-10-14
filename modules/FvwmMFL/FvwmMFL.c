/* FvwmMFL -- Fvwm3 Module Front Loader
 *
 * This program accepts listeners over a UDS for the purposes of receiving
 * information from FVWM3.
 *
 * Released under the same license as FVWM3 itself.
 */

#include "config.h"

#include "fvwm/fvwm.h"

#include "libs/Module.h"
#include "libs/fd_safety.h"
#include "libs/safemalloc.h"
#include "libs/queue.h"
#include "libs/fvwmsignal.h"
#include "libs/vpacket.h"
#include "libs/getpwuid.h"
#include "libs/envvar.h"
#include "libs/cJSON.h"
#include "event_flags.h"
#include "stream.h"

#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

#ifdef HOST_MACOS
#include <event.h>
#else
#include <event2/event.h>
#endif

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/util.h>

#define MYNAME "FvwmMFL"

static int debug;
struct fvwm_msg;
static char *socket_name, *socket_basepath;
static char *lockfile = NULL;
static int fvwmmfl_lock_fd = -1;

static unsigned int client_count;

struct client {
	struct event *resume;
	struct event *partial;
	struct bufferevent	*comms;
	unsigned long		 flags_m;
	unsigned long		 flags_mx;

	TAILQ_ENTRY(client)	 entry;
};

TAILQ_HEAD(clients, client);
struct clients  clientq;

struct fvwm_comms {
	int		 fd[2];
	struct event_base *base;
	struct bufferevent *input, *output;
	struct event *resume;
	ModuleArgs	*m;
};
struct fvwm_comms	 fc;

struct fvwm_msg {
	cJSON	*j_obj;
	char	*msg;
};

struct event_flag {
	const char	*event;
	unsigned long	 flag;
} etf[] = {
	{"new_window", M_ADD_WINDOW},
	{"configure_window", M_CONFIGURE_WINDOW},
	{"new_page", M_NEW_PAGE},
	{"new_desk", M_NEW_DESK},
	{"raise_window", M_RAISE_WINDOW},
	{"lower_window", M_LOWER_WINDOW},
	{"focus_change", M_FOCUS_CHANGE},
	{"destroy_window", M_DESTROY_WINDOW},
	{"iconify", M_ICONIFY},
	{"deiconify", M_DEICONIFY},
	{"window_name", M_WINDOW_NAME},
	{"visible_name", M_VISIBLE_NAME},
	{"icon_name", M_ICON_NAME},
	{"res_class", M_RES_CLASS},
	{"res_name", M_RES_NAME},
	{"icon_location", M_ICON_LOCATION},
	{"map", M_MAP},
	{"icon_file", M_ICON_FILE},
	{"window_shade", M_WINDOWSHADE},
	{"dewindow_shade", M_DEWINDOWSHADE},
	{"restack", M_RESTACK},
	{"leave_window", MX_LEAVE_WINDOW|M_EXTENDED_MSG},
	{"enter_window", MX_ENTER_WINDOW|M_EXTENDED_MSG},
	{"visible_icon_name", MX_VISIBLE_ICON_NAME|M_EXTENDED_MSG},
	{"echo", MX_ECHO|M_EXTENDED_MSG},
};


static void fvwm_read(struct bufferevent *, void *);
static void client_destroy(struct client *);
static void broadcast_to_client(FvwmPacket *);
static void setup_signal_handlers(struct event_base *);
static inline const char *flag_to_event(unsigned long);
static void HandleTerminate(int, short, void *);
static int client_set_interest(struct client *, const char *);
static struct fvwm_msg *handle_packet(unsigned long, unsigned long *, unsigned long);
static struct fvwm_msg *fvwm_msg_new(void);
static void fvwm_msg_free(struct fvwm_msg *);
static void register_interest(void);
static void send_version_info(struct client *);
static void set_pathnames(void);
static int lock_fvwmmfl(void);
static void fm_to_json(struct fvwm_msg *);

static void
fm_to_json(struct fvwm_msg *fm)
{
	if ((fm->msg = cJSON_PrintUnformatted(fm->j_obj)) == NULL) {
		cJSON_Delete(fm->j_obj);
		fm->j_obj = NULL;
	}
}

static struct fvwm_msg *
fvwm_msg_new(void)
{
	struct fvwm_msg		*fm;

	fm = fxcalloc(1, sizeof *fm);

	if ((fm->j_obj = cJSON_CreateObject()) == NULL) {
		fprintf(stderr, "FvwmMFL: couldn't initialise JSON struct\n");
		exit(1);
	}
	return (fm);
}

static void
fvwm_msg_free(struct fvwm_msg *fm)
{
	if (fm == NULL)
		return;
	cJSON_free(fm->msg);
	cJSON_Delete(fm->j_obj);
	free(fm);
}

static void
HandleTerminate(int fd, short what, void *arg)
{
	fprintf(stderr, "%s: dying...\n", __func__);
	event_base_loopexit(arg, NULL);
	fvwmSetTerminate(fd);
}

static void
setup_signal_handlers(struct event_base *base)
{
	struct event	*hup, *term, *intrp, *quit, *child;

	signal(SIGPIPE, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);

	hup   =  evsignal_new(base, SIGHUP,  HandleTerminate, base);
	term  =  evsignal_new(base, SIGTERM, HandleTerminate, base);
	quit  =  evsignal_new(base, SIGQUIT, HandleTerminate, base);
	child =  evsignal_new(base, SIGCHLD, HandleTerminate, base);
	intrp =  evsignal_new(base, SIGINT,  HandleTerminate, base);

	evsignal_add(hup, NULL);
	evsignal_add(term, NULL);
	evsignal_add(quit, NULL);
	evsignal_add(child, NULL);
	evsignal_add(intrp, NULL);
}

static void
send_version_info(struct client *c)
{
	struct fvwm_msg		*fm;
	cJSON			*content;

	fm = fvwm_msg_new();

	content = cJSON_CreateObject();
	cJSON_AddStringToObject(content, "version", VERSION);
	cJSON_AddStringToObject(content, "version_info", VERSIONINFO);
	cJSON_AddItemToObject(fm->j_obj, "connection_profile", content);

	fm_to_json(fm);
	if (fm->msg == NULL) {
		fvwm_msg_free(fm);
		return;
	}

	/* This small startup record fits the empty output queue. */
	mfl_queue_line(bufferevent_get_output(c->comms), fm->msg);
	fvwm_msg_free(fm);
}

static struct fvwm_msg *
handle_packet(unsigned long type, unsigned long *body, unsigned long len)
{
	struct fvwm_msg		*fm = NULL;
	const char		*type_name = flag_to_event(type);
	char			 xwid[20];
	cJSON			*content;
	size_t			 body_len;

	if (len < FvwmPacketHeaderSize)
		return (NULL);
	body_len = len - FvwmPacketHeaderSize;

	if (type_name == NULL || body_len == 0) {
		fprintf(stderr, "Couldn't find type_name\n");
		goto out;
	}

	fm = fvwm_msg_new();

	snprintf(xwid, sizeof(xwid), "0x%lx", body[0]);

	if (debug)
		fprintf(stderr, "%s: matched: <<%s>>\n", __func__, type_name);

	switch(type) {
	case MX_ENTER_WINDOW:
	case MX_LEAVE_WINDOW: {
		if ((content = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddStringToObject(content, "window", xwid);
		cJSON_AddItemToObject(fm->j_obj, type_name, content);

		fm_to_json(fm);
		return (fm);
	}
	case MX_ECHO: {
		if (body_len < 4 ||
		    memchr(&body[3], '\0',
			(body_len - 3) * sizeof(unsigned long)) == NULL)
			goto out;
		if ((content = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddStringToObject(content, "message", (char *)&body[3]);
		cJSON_AddItemToObject(fm->j_obj, type_name, content);

		fm_to_json(fm);
		return (fm);
	}
	case M_ADD_WINDOW:
	case M_CONFIGURE_WINDOW: {
		struct ConfigWinPacket	*cwp = (void *)body;
		cJSON			*hints, *ewmh, *frame;

		if (body_len * sizeof(unsigned long) < sizeof(*cwp))
			goto out;

		if ((content = cJSON_CreateObject()) == NULL)
			goto out;

		cJSON_AddStringToObject(content, "window", xwid);
		cJSON_AddNumberToObject(content, "title_height", cwp->title_height);
		cJSON_AddNumberToObject(content, "border_width", cwp->border_width);

		if ((frame = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddNumberToObject(frame, "window", cwp->frame);
		cJSON_AddNumberToObject(frame, "x", cwp->frame_x);
		cJSON_AddNumberToObject(frame, "y", cwp->frame_y);
		cJSON_AddNumberToObject(frame, "width", cwp->frame_width);
		cJSON_AddNumberToObject(frame, "height", cwp->frame_height);
		cJSON_AddItemToObject(content, "frame", frame);

		if ((hints = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddNumberToObject(hints, "base_width", cwp->hints_base_width);
		cJSON_AddNumberToObject(hints, "base_height", cwp->hints_base_height);
		cJSON_AddNumberToObject(hints, "inc_width", cwp->hints_width_inc);
		cJSON_AddNumberToObject(hints, "inc_height", cwp->hints_height_inc);
		cJSON_AddNumberToObject(hints, "orig_inc_width", cwp->orig_hints_width_inc);
		cJSON_AddNumberToObject(hints, "orig_inc_height", cwp->orig_hints_height_inc);
		cJSON_AddNumberToObject(hints, "min_width", cwp->hints_min_width);
		cJSON_AddNumberToObject(hints, "min_height", cwp->hints_min_height);
		cJSON_AddNumberToObject(hints, "max_width", cwp->hints_max_width);
		cJSON_AddNumberToObject(hints, "max_height", cwp->hints_max_height);
		cJSON_AddItemToObject(content, "hints", hints);

		if ((ewmh = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddNumberToObject(ewmh, "layer", cwp->ewmh_hint_layer);
		cJSON_AddNumberToObject(ewmh, "desktop", cwp->ewmh_hint_desktop);
		cJSON_AddNumberToObject(ewmh, "window_type", cwp->ewmh_window_type);
		cJSON_AddItemToObject(content, "ewmh", ewmh);

		cJSON_AddItemToObject(fm->j_obj, type_name, content);
		fm_to_json(fm);
		return (fm);
	}
	case M_MAP:
	case M_LOWER_WINDOW:
	case M_RAISE_WINDOW:
	case M_DESTROY_WINDOW: {
		if ((content = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddStringToObject(content, "window", xwid);
		cJSON_AddItemToObject(fm->j_obj, type_name, content);

		fm_to_json(fm);
		return (fm);
	}
	case M_FOCUS_CHANGE: {
		cJSON	*hilight;

		if (body_len < 5)
			goto out;

		if ((content = cJSON_CreateObject()) == NULL)
			goto out;

		cJSON_AddStringToObject(content, "window", xwid);
		cJSON_AddNumberToObject(content, "type", body[2]);

		if ((hilight = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddNumberToObject(hilight, "text_colour", body[3]);
		cJSON_AddNumberToObject(hilight, "bg_colour", body[4]);
		cJSON_AddItemToObject(content, "hilight", hilight);

		cJSON_AddItemToObject(fm->j_obj, type_name, content);

		fm_to_json(fm);
		return (fm);
	}
	case M_WINDOW_NAME:
	case M_VISIBLE_NAME:
	case MX_VISIBLE_ICON_NAME:
	case M_ICON_NAME:
	case M_RES_CLASS:
	case M_RES_NAME: {
		if (body_len < 4 ||
		    memchr(&body[3], '\0',
			(body_len - 3) * sizeof(unsigned long)) == NULL)
			goto out;
		if ((content = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddStringToObject(content, "window", xwid);
		cJSON_AddStringToObject(content, "name", (char *)&body[3]);

		cJSON_AddItemToObject(fm->j_obj, type_name, content);

		fm_to_json(fm);
		return (fm);
	}
	case M_NEW_DESK: {
		if (body_len < 2)
			goto out;
		if ((content = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddNumberToObject(content, "desk", body[0]);
		cJSON_AddNumberToObject(content, "monitor_id", body[1]);

		cJSON_AddItemToObject(fm->j_obj, type_name, content);

		fm_to_json(fm);
		return (fm);
	}
	case M_NEW_PAGE: {
		cJSON	*virtual, *display;

		if (body_len < 8)
			goto out;

		if ((content = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddNumberToObject(content, "monitor_id", body[7]);

		if ((virtual = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddNumberToObject(virtual, "vx", body[0]);
		cJSON_AddNumberToObject(virtual, "vy", body[1]);
		cJSON_AddNumberToObject(virtual, "vx_pages", body[5]);
		cJSON_AddNumberToObject(virtual, "vy_pages", body[6]);
		cJSON_AddNumberToObject(virtual, "current_desk", body[2]);
		cJSON_AddItemToObject(content, "virtual_scr", virtual);

		if ((display = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddNumberToObject(display, "width", body[3]);
		cJSON_AddNumberToObject(display, "height", body[4]);
		cJSON_AddItemToObject(content, "display", display);

		cJSON_AddItemToObject(fm->j_obj, type_name, content);

		fm_to_json(fm);
		return (fm);
	}
	case M_ICON_LOCATION: {
		if (body_len < 7)
			goto out;
		if ((content = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddStringToObject(content, "window", xwid);
		cJSON_AddNumberToObject(content, "x", body[3]);
		cJSON_AddNumberToObject(content, "y", body[4]);
		cJSON_AddNumberToObject(content, "width", body[5]);
		cJSON_AddNumberToObject(content, "height", body[6]);

		cJSON_AddItemToObject(fm->j_obj, type_name, content);

		fm_to_json(fm);
		return (fm);
	}
	case M_ICONIFY: {
		cJSON	*icon, *frame;

		if (body_len < 7)
			goto out;

		if ((content = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddStringToObject(content, "window", xwid);

		if ((icon = cJSON_CreateObject()) == NULL)
			goto out;
		cJSON_AddNumberToObject(icon, "x", body[3]);
		cJSON_AddNumberToObject(icon, "y", body[4]);
		cJSON_AddNumberToObject(icon, "width", body[5]);
		cJSON_AddNumberToObject(icon, "height", body[6]);
		cJSON_AddItemToObject(content, "icon", icon);

		if (body_len >= 11) {
			if ((frame = cJSON_CreateObject()) == NULL)
				goto out;
			cJSON_AddNumberToObject(frame, "x", body[7]);
			cJSON_AddNumberToObject(frame, "y", body[8]);
			cJSON_AddNumberToObject(frame, "width", body[9]);
			cJSON_AddNumberToObject(frame, "height", body[10]);
			cJSON_AddItemToObject(content, "frame", frame);
		}

		cJSON_AddItemToObject(fm->j_obj, type_name, content);

		fm_to_json(fm);
		return (fm);
	}
	default:
		goto out;
	}
out:
	fvwm_msg_free(fm);
	return (NULL);
}

static inline const char *
flag_to_event(unsigned long flag)
{
	size_t	 i;

	for (i = 0; i < (sizeof(etf) / sizeof(etf[0])); i++) {
		if (fvwmmfl_event_flag_matches(flag, etf[i].flag))
			return (etf[i].event);
	}

	return (NULL);
}

#define EFLAGSET	0x1
#define EFLAGUNSET	0x2

static int
client_set_interest(struct client *c, const char *event)
{
	size_t		 i;
	int		 flag_type = -1;
	bool		 changed = false;
#define PRESET "set"
#define PREUNSET "unset"

	if (strncmp(event, PRESET " ", sizeof(PRESET)) == 0) {
		event += strlen(PRESET) + 1;
		flag_type = EFLAGSET;
	} else if (strncmp(event, PREUNSET " ", sizeof(PREUNSET)) == 0) {
		event += strlen(PREUNSET) + 1;
		flag_type = EFLAGUNSET;
	}

	if (strcmp(event, "debug") == 0) {
		debug = (flag_type == EFLAGSET) ? 1 : 0;

		/* Never send to FVWM3. */
		return (true);
	}

	for (i = 0; i < (sizeof(etf) / sizeof(etf[0])); i++) {
		if (strcmp(etf[i].event, event) == 0) {
			unsigned long	 f = etf[i].flag;

			changed = true;
			if (flag_type == EFLAGSET) {
				if (f & M_EXTENDED_MSG) {
					if (debug)
						fprintf(stderr,
						    "Setting %s\n", event);
					c->flags_mx |= f;
				} else {
					if (debug)
						fprintf(stderr,
						    "Setting %s\n", event);
					c->flags_m |= f;
				}
			} else {
				if (f & M_EXTENDED_MSG) {
					if (debug)
						fprintf(stderr,
						    "Unsetting %s\n", event);
					c->flags_mx = fvwmmfl_event_flag_remove(c->flags_mx, f);
				} else {
					if (debug)
						fprintf(stderr,
						   "Unsetting %s\n", event);
					c->flags_m &= ~f;
				}
			}
		}
	}
	return (changed);
}

static void
register_interest(void)
{
	size_t		i;
	unsigned long	f_m = 0, f_mx = 0;

	for (i = 0; i < (sizeof(etf) / sizeof(etf[0])); i++) {
		unsigned long f = etf[i].flag;
		if (f & M_EXTENDED_MSG)
			f_mx |= f;
		else
			f_m |=f;
	}

	if (debug) {
		fprintf(stderr, "Sending: flags_m:  %lu\n", f_m);
		fprintf(stderr, "Sending: flags_mx: %lu\n", f_mx);
	}
	char command[64];
	snprintf(command, sizeof(command), "SET_MASK %lu", f_m);
	mfl_queue_command(bufferevent_get_output(fc.output), command);
	snprintf(command, sizeof(command), "SET_MASK %lu", f_mx);
	mfl_queue_command(bufferevent_get_output(fc.output), command);
}

static void
broadcast_to_client(FvwmPacket *packet)
{
	struct client		*c, *next;
	struct fvwm_msg		*fm = NULL;
	unsigned long		*body = packet->body;
	unsigned long		 type =	packet->type;
	unsigned long		 length = packet->size;
	bool			 is_extended = (type & M_EXTENDED_MSG);
	bool			 enact_change = false;

	for (c = TAILQ_FIRST(&clientq); c != NULL; c = next) {
		next = TAILQ_NEXT(c, entry);
		enact_change = is_extended ?
			(c->flags_mx & type) == type : (c->flags_m & type) != 0;
		/* This packet really isn't for this client; move on to the
		 * next client.
		 */
		if (!enact_change)
			continue;

		/* Event payloads are identical for all subscribers. Serialize once. */
		if (fm == NULL) {
			fm = handle_packet(type, body, length);
			if (fm == NULL || fm->msg == NULL)
				break;
		}
		if (!mfl_queue_line(bufferevent_get_output(c->comms), fm->msg)) {
			/* Contain stalled readers to their own connection. */
			client_destroy(c);
		}
	}
	fvwm_msg_free(fm);
}

static void
client_destroy(struct client *c)
{
	TAILQ_REMOVE(&clientq, c, entry);
	client_count--;
	event_free(c->resume);
	event_free(c->partial);
	bufferevent_free(c->comms);
	free(c);
}

static void
client_read_cb(struct bufferevent *bev, void *ctx)
{
	struct client *c = ctx;
	struct evbuffer *input = bufferevent_get_input(bev);
	char command[MAX_MODULE_INPUT_TEXT_LEN + 1];
	const struct timeval timeout = {MFL_PROGRESS_TIMEOUT, 0};
	const struct timeval yield = {0, 1000};
	unsigned int count;

	event_del(c->resume);
	for (count = 0; count < MFL_COMMAND_BUDGET; count++)
	{
		enum mfl_stream_result result = mfl_read_command(input, command);
		if (result == MFL_STREAM_INVALID)
		{
			client_destroy(c);
			return;
		}
		if (result == MFL_STREAM_INCOMPLETE)
		{
			/* Idle subscribers are allowed; partial lines must progress. */
			if (evbuffer_get_length(input) == 0)
				event_del(c->partial);
			else if (!evtimer_pending(c->partial, NULL))
				evtimer_add(c->partial, &timeout);
			return;
		}
		event_del(c->partial);
		if (command[0] == '\0' || client_set_interest(c, command))
			continue;
		if (!mfl_queue_command(bufferevent_get_output(fc.output), command))
		{
			client_destroy(c);
			return;
		}
	}
	/* A timer yields to pipe I/O and other clients before continuing. */
	if (evbuffer_get_length(input) != 0)
		evtimer_add(c->resume, &yield);
}

static void
client_partial_timeout_cb(evutil_socket_t fd, short events, void *ctx)
{
	client_destroy(ctx);
}

static void
client_resume_cb(evutil_socket_t fd, short events, void *ctx)
{
	struct client *c = ctx;
	client_read_cb(c->comms, c);
}

static void
client_err_cb(struct bufferevent *bev, short events, void *ctx)
{
	if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR | BEV_EVENT_TIMEOUT))
		client_destroy(ctx);
}

static void
accept_conn_cb(struct evconnlistener *l, evutil_socket_t fd,
		struct sockaddr *add, int socklen, void *ctx)
{
	struct client *c;
	struct event_base *base = evconnlistener_get_base(l);
	const struct timeval timeout = {MFL_PROGRESS_TIMEOUT, 0};

	if (client_count >= MFL_CLIENT_LIMIT ||
	    evutil_make_socket_closeonexec(fd) < 0)
	    {
		close(fd);
		return;
	}
	c = fxcalloc(1, sizeof(*c));
	c->comms = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	c->resume = evtimer_new(base, client_resume_cb, c);
	c->partial = evtimer_new(base, client_partial_timeout_cb, c);
	if (c->comms == NULL || c->resume == NULL || c->partial == NULL)
	{
		if (c->comms != NULL)
			bufferevent_free(c->comms);
		else
			close(fd);
		if (c->resume != NULL)
			event_free(c->resume);
		if (c->partial != NULL)
			event_free(c->partial);
		free(c);
		return;
	}
	bufferevent_setcb(c->comms, client_read_cb, NULL, client_err_cb, c);
	bufferevent_setwatermark(c->comms, EV_READ, 0, MFL_INPUT_LIMIT);
	bufferevent_set_timeouts(c->comms, NULL, &timeout);
	TAILQ_INSERT_TAIL(&clientq, c, entry);
	client_count++;
	send_version_info(c);
	bufferevent_enable(c->comms, EV_READ | EV_WRITE);
}

static void accept_error_cb(struct evconnlistener *listener, void *ctx)
{
	struct event_base *base = evconnlistener_get_base(listener);
	int err = EVUTIL_SOCKET_ERROR();
	fprintf(stderr, "Got an error %d (%s) on the listener. "
		"Shutting down.\n", err, evutil_socket_error_to_string(err));

	event_base_loopexit(base, NULL);
}

static void
fvwm_read(struct bufferevent *bev, void *data)
{
	unsigned long packet[FvwmPacketMaxSize];
	const struct timeval yield = {0, 1000};
	unsigned int count;

	event_del(fc.resume);
	for (count = 0; count < MFL_PACKET_BUDGET; count++)
	{
		enum mfl_stream_result result = mfl_read_packet(
		    bufferevent_get_input(bev), packet);
		if (result == MFL_STREAM_INCOMPLETE)
			return;
		if (result == MFL_STREAM_INVALID)
		{
			event_base_loopexit(fc.base, NULL);
			return;
		}
		broadcast_to_client((FvwmPacket *)packet);
	}
	if (evbuffer_get_length(bufferevent_get_input(bev)) != 0)
		evtimer_add(fc.resume, &yield);
}

static void
fvwm_resume_cb(evutil_socket_t fd, short events, void *data)
{
	fvwm_read(fc.input, NULL);
}

static void
fvwm_error_cb(struct bufferevent *bev, short events, void *data)
{
	if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR | BEV_EVENT_TIMEOUT))
		event_base_loopexit(fc.base, NULL);
}

void
set_pathnames(void)
{
	char		*mflsock_env, *display_env, *tmpdir, *display_name;
	const char	*unrolled_path;
	struct stat	 sb;
	size_t		 i;

	/* Figure out if we are using default MFL socket path or we should
	 * respect environment variable FVWMMFL_SOCKET for FvwmMFL socket path
	 */

	mflsock_env = getenv("FVWMMFL_SOCKET_PATH");
	display_env = getenv("DISPLAY");

	if (mflsock_env == NULL) {
		if ((tmpdir = getenv("XDG_RUNTIME_DIR")) != NULL) {
			xasprintf(&socket_basepath, "%s/fvwmmfl", tmpdir);
		} else {
			if ((tmpdir = getenv("TMPDIR")) == NULL)
				tmpdir = "/tmp";
			xasprintf(&socket_basepath, "%s/fvwmmfl-%lu", tmpdir,
			    (unsigned long)getuid());
		}
		goto check_dir;
	}

	unrolled_path = expand_path(mflsock_env);
	if (unrolled_path[0] == '/')
		socket_basepath = fxstrdup(unrolled_path);
	else {
		xasprintf(&socket_basepath, "%s/%s", getenv("FVWM_USERDIR"),
			unrolled_path);
	}

	free((void *)unrolled_path);

check_dir:
	if (lstat(socket_basepath, &sb) == 0) {
		if (!S_ISDIR(sb.st_mode) || sb.st_uid != getuid() ||
		    (sb.st_mode & 077) != 0) {
			fprintf(stderr, "Unsafe FvwmMFL socket directory: %s\n",
			    socket_basepath);
			free(socket_basepath);
			exit(1);
		}
	} else if (errno != ENOENT) {
		fprintf(stderr, "Couldn't inspect socket_basepath: %s: %s\n",
		    socket_basepath, strerror(errno));
		free(socket_basepath);
		exit(1);
	} else if (mkdir(socket_basepath, S_IRWXU) != 0) {
		fprintf(stderr, "Couldn't create socket_basepath: %s: %s\n",
			socket_basepath, strerror(errno));
		free(socket_basepath);
		exit(1);
	}

	display_name = fxstrdup(display_env != NULL ? display_env : "default");
	for (i = 0; display_name[i] != '\0'; i++) {
		if (!isalnum((unsigned char)display_name[i]) &&
		    display_name[i] != '-' && display_name[i] != '_')
			display_name[i] = '_';
	}
	xasprintf(&socket_name, "%s/fvwm_mfl_%s.sock", socket_basepath,
		display_name);

	xasprintf(&lockfile, "%s/fvwmmfl_lockfile_%s.lock", socket_basepath,
	   display_name);
	free(display_name);

}

int
lock_fvwmmfl(void)
{
	struct flock	l;
	int		fd = -1;

	if ((fd = fvwm_open_private_lock(lockfile)) == -1)
		return fd;
	l.l_start = 0;
	l.l_type = F_WRLCK;
	l.l_len = 0;
	l.l_whence = SEEK_SET;

	if (fcntl(fd, F_SETLK, &l) == -1) {
		close(fd);
		return -1;
	}

	return fd;
}

int main(int argc, char **argv)
{
	struct event_base     *base;
	struct evconnlistener *fmd_cfd;
	struct sockaddr_un    sin;
	char		     *pe;
	size_t		      socket_name_len;

	TAILQ_INIT(&clientq);

	if ((fc.m = ParseModuleArgs(argc, argv, 1)) == NULL) {
		fprintf(stderr, "%s must be started by FVWM3\n", MYNAME);
		return (1);
	}

	set_pathnames();

	/* If we're already running... */
	if ((fvwmmfl_lock_fd = lock_fvwmmfl()) == -1)
	{
		perror("FvwmMFL lock");
		return (1);
	}

	unlink(socket_name);

	/* Create new event base */
	if ((base = event_base_new()) == NULL) {
		fprintf(stderr, "Couldn't start libevent\n");
		return (1);
	}
	setup_signal_handlers(base);

	memset(&sin, 0, sizeof(sin));
	sin.sun_family = AF_LOCAL;
	socket_name_len = strlen(socket_name);
	if (socket_name_len >= sizeof(sin.sun_path)) {
		fprintf(stderr, "FvwmMFL socket path is too long: %s\n",
		    socket_name);
		return (1);
	}
	memcpy(sin.sun_path, socket_name, socket_name_len + 1);

	/* Create a new listener */
	fmd_cfd = evconnlistener_new_bind(base, accept_conn_cb, NULL,
		  LEV_OPT_CLOSE_ON_FREE | LEV_OPT_CLOSE_ON_EXEC, -1,
		  (struct sockaddr *)&sin, sizeof(sin));
	if (fmd_cfd == NULL) {
		perror("Couldn't create listener");
		return 1;
	}
	evconnlistener_set_error_cb(fmd_cfd, accept_error_cb);

	chmod(socket_name, 0600);

	/* Setup comms to fvwm3. */
	fc.fd[0] = fc.m->to_fvwm;
	fc.fd[1] = fc.m->from_fvwm;

	fc.base = base;
	if (evutil_make_socket_nonblocking(fc.fd[0]) < 0 ||
	    evutil_make_socket_nonblocking(fc.fd[1]) < 0 ||
	    evutil_make_socket_closeonexec(fc.fd[0]) < 0 ||
	    evutil_make_socket_closeonexec(fc.fd[1]) < 0)
		err(1, "Cannot prepare module pipes");
	fc.input = bufferevent_socket_new(base, fc.fd[1], BEV_OPT_CLOSE_ON_FREE);
	fc.output = bufferevent_socket_new(base, fc.fd[0], BEV_OPT_CLOSE_ON_FREE);
	fc.resume = evtimer_new(base, fvwm_resume_cb, NULL);
	if (fc.input == NULL || fc.output == NULL || fc.resume == NULL)
		err(1, "Cannot create module transport");
	bufferevent_setcb(fc.input, fvwm_read, NULL, fvwm_error_cb, NULL);
	bufferevent_setwatermark(fc.input, EV_READ, 0, MFL_INPUT_LIMIT);
	bufferevent_setcb(fc.output, NULL, NULL, fvwm_error_cb, NULL);
	const struct timeval timeout = {MFL_PROGRESS_TIMEOUT, 0};
	bufferevent_set_timeouts(fc.output, NULL, &timeout);
	bufferevent_enable(fc.input, EV_READ);
	bufferevent_enable(fc.output, EV_WRITE);

	/* If the user hasn't set FVWMMFL_SOCKET_PATH in the environment, then
	 * tell Fvwm3 to do this.  Note that we have to get Fvwm3 to do this
	 * so that all other windows can inherit this environment variable.
	 * We cannot use flib_putenv() here since this never reaches Fvwm3's
	 * environment.
	 */
	if (getenv("FVWMMFL_SOCKET_PATH") == NULL) {

		xasprintf(&pe, "SetEnv FVWMMFL_SOCKET_PATH %s", socket_basepath);
		if (!mfl_queue_command(bufferevent_get_output(fc.output), pe))
			err(1, "Startup command exceeds transport limits");
		free(pe);
	}

	xasprintf(&pe, "SetEnv FVWMMFL_SOCKET %s", socket_name);
	if (!mfl_queue_command(bufferevent_get_output(fc.output), pe))
			err(1, "Startup command exceeds transport limits");
	free(pe);

	register_interest();

	mfl_queue_command(bufferevent_get_output(fc.output),
	    ModuleFinishedStartupResponse);

	event_base_dispatch(base);
	while (!TAILQ_EMPTY(&clientq))
		client_destroy(TAILQ_FIRST(&clientq));
	event_free(fc.resume);
	bufferevent_free(fc.input);
	bufferevent_free(fc.output);
	evconnlistener_free(fmd_cfd);
	unlink(socket_name);
	/* Retain the locked inode: unlinking lets a new instance lock a
	 * different file while this process still owns the original lock. */

	return (0);
}
