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
#include "libs/safemalloc.h"
#include "libs/queue.h"
#include "libs/fvwmsignal.h"
#include "libs/vpacket.h"
#include "libs/getpwuid.h"
#include "libs/envvar.h"
#include "libs/cJSON.h"

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

/* FIXME: including event_struct.h won't be binary comaptible with future
 * versions of libevent.
 */
#include <event2/event_struct.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_compat.h>
#include <event2/listener.h>
#include <event2/util.h>

#define MYNAME "FvwmMFL"

static int debug;
struct fvwm_msg;
static char *socket_name, *socket_basepath;
static char *lockfile = NULL;
static int fvwmmfl_lock_fd = -1;

struct client {
	struct bufferevent	*comms;
	unsigned long		 flags_m;
	unsigned long		 flags_mx;

	TAILQ_ENTRY(client)	 entry;
};

TAILQ_HEAD(clients, client);
struct clients  clientq;

struct fvwm_comms {
	int		 fd[2];
	struct event	*read_ev;
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


static void fvwm_read(int, short, void *);
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
	unlink(lockfile);
	unlink(socket_name);
	rmdir(socket_basepath);
	fvwmSetTerminate(fd);
}

static void
setup_signal_handlers(struct event_base *base)
{
	struct event	*hup, *term, *intrp, *quit, *child;

	signal(SIGPIPE, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);

	hup   =  evsignal_new(base, SIGHUP,  HandleTerminate, NULL);
	term  =  evsignal_new(base, SIGTERM, HandleTerminate, NULL);
	quit  =  evsignal_new(base, SIGQUIT, HandleTerminate, NULL);
	child =  evsignal_new(base, SIGCHLD, HandleTerminate, NULL);
	intrp =  evsignal_new(base, SIGINT,  HandleTerminate, NULL);

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
	char			*to_client;

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

	/* Ensure there's a newline at the end of the string, so that
	 * buffered output can be sent.
	 */
	xasprintf(&to_client, "%s\n", fm->msg);

	bufferevent_write(c->comms, to_client, strlen(to_client));
	bufferevent_flush(c->comms, EV_WRITE, BEV_NORMAL);
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
	bool is_extended = (flag & M_EXTENDED_MSG);

	for (i = 0; i < (sizeof(etf) / sizeof(etf[0])); i++) {
		int f = etf[i].flag;
		if (is_extended && (f & M_EXTENDED_MSG) && (f == flag))
			return (etf[i].event);
		if (!is_extended && (f & flag))
			return (etf[i].event);
	}

	return (NULL);
}

static inline bool
strsw(const char *pre, const char *str)
{
	return (strncmp(pre, str, strlen(pre)) == 0);
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

	if (strsw(PRESET, event)) {
		event += strlen(PRESET) + 1;
		flag_type = EFLAGSET;
	} else if (strsw(PREUNSET, event)) {
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
					c->flags_mx &= ~f;
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
	SetMessageMask(fc.fd, f_m);
	SetMessageMask(fc.fd, f_mx);
}

static void
broadcast_to_client(FvwmPacket *packet)
{
	struct client		*c;
	struct fvwm_msg		*fm;
	char			*to_client;
	unsigned long		*body = packet->body;
	unsigned long		 type =	packet->type;
	unsigned long		 length = packet->size;
	bool			 is_extended = (type & M_EXTENDED_MSG);
	bool			 enact_change = false;

	TAILQ_FOREACH(c, &clientq, entry) {
		enact_change = is_extended ?
			(c->flags_mx & type) == type : (c->flags_m & type) != 0;
		/* This packet really isn't for this client; move on to the
		 * next client.
		 */
		if (!enact_change)
			continue;

		if ((fm = handle_packet(type, body, length)) == NULL) {
			fprintf(stderr, "Packet was NULL...\n");
			continue;
		}

		if (fm->msg == NULL) {
			fvwm_msg_free(fm);
			continue;
		}
		/* Ensure there's a newline at the end of the string, so that
		 * buffered output can be sent.
		 */
		xasprintf(&to_client, "%s\n", fm->msg);

		bufferevent_write(c->comms, to_client, strlen(to_client));
		bufferevent_flush(c->comms, EV_WRITE, BEV_NORMAL);
		fvwm_msg_free(fm);
		free(to_client);
	}
}

static void
client_read_cb(struct bufferevent *bev, void *ctx)
{
	struct evbuffer	*input = bufferevent_get_input(bev);
	struct client	*c =	 ctx;
	size_t		 len = evbuffer_get_length(input);
	char		 *data = fxcalloc(len + 1, sizeof (char));
	char		 *split;

	evbuffer_remove(input, data, len);

	if (debug)
		fprintf(stderr, "%s: DATA: <%s>, LEN: <%zu>\n", __func__, data, len);

	/* Remove the newline if there is one. */
	if (len > 0 && data[len - 1] == '\n')
		data[len - 1] = '\0';

	if (*data == '\0')
		goto out;

	split = strtok(data, "\n");
	while (split != NULL) {
		if (client_set_interest(c, split)) {
			split = strtok(NULL, "\n");
			continue;
		}
		SendText(fc.fd, split, 0);
		split = strtok(NULL, "\n");
	}
out:
	free(data);
}

static void
client_write_cb(struct bufferevent *bev, void *ctx)
{
	struct client	*c = ctx;

	if (debug)
		fprintf(stderr, "Writing... (client %p)...\n", c);
}

static void client_err_cb(struct bufferevent *bev, short events, void *ctx)
{
	struct client	*c = ctx;

	if (events & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
		TAILQ_REMOVE(&clientq, c, entry);
		bufferevent_free(bev);
		free(c);
	}
}


static void
accept_conn_cb(struct evconnlistener *l, evutil_socket_t fd,
		struct sockaddr *add, int socklen, void *ctx)
{
	/* We got a new connection! Set up a bufferevent for it. */
	struct client		*c;
	struct event_base	*base = evconnlistener_get_base(l);

	c = fxmalloc(sizeof *c);

	c->comms = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (c->comms == NULL) {
		close(fd);
		free(c);
		return;
	}
	c->flags_m = 0;
	c->flags_mx = 0;

	bufferevent_setcb(c->comms, client_read_cb, client_write_cb,
	    client_err_cb, c);
	bufferevent_enable(c->comms, EV_READ|EV_WRITE|EV_PERSIST);

	send_version_info(c);

	TAILQ_INSERT_TAIL(&clientq, c, entry);
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
fvwm_read(int efd, short ev, void *data)
{
	FvwmPacket	*packet;

	if ((packet = ReadFvwmPacket(efd)) == NULL) {
		if (debug)
			fprintf(stderr, "Couldn't read from FVWM - exiting.\n");
		unlink(lockfile);
		unlink(socket_name);
		rmdir(socket_basepath);
		exit(0);
	}

	broadcast_to_client(packet);
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

	if ((fd = open(lockfile, O_CREAT|O_WRONLY, 0600)) == -1)
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
		return (1);

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
		  LEV_OPT_CLOSE_ON_FREE, -1,
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

	/* If the user hasn't set FVWMMFL_SOCKET_PATH in the environment, then
	 * tell Fvwm3 to do this.  Note that we have to get Fvwm3 to do this
	 * so that all other windows can inherit this environment variable.
	 * We cannot use flib_putenv() here since this never reaches Fvwm3's
	 * environment.
	 */
	if (getenv("FVWMMFL_SOCKET_PATH") == NULL) {

		xasprintf(&pe, "SetEnv FVWMMFL_SOCKET_PATH %s", socket_basepath);
		SendText(fc.fd, pe, 0);
		free(pe);
	}

	xasprintf(&pe, "SetEnv FVWMMFL_SOCKET %s", socket_name);
	SendText(fc.fd, pe, 0);
	free(pe);

	if (evutil_make_socket_nonblocking(fc.fd[0]) < 0)
		fprintf(stderr, "fvwm to_fvwm socket non-blocking failed");
	if (evutil_make_socket_nonblocking(fc.fd[1]) < 0)
		fprintf(stderr, "fvwm to_fvwm socket non-blocking failed");

	fc.read_ev = event_new(base, fc.fd[1], EV_READ|EV_PERSIST, fvwm_read, NULL);
	event_add(fc.read_ev, NULL);

	register_interest();

	SendFinishedStartupNotification(fc.fd);

	event_base_dispatch(base);
	unlink(socket_name);

	return (0);
}
