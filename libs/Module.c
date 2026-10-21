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
** Module.c: code for modules to communicate with fvwm
*/
#include "config.h"
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include "libs/defaults.h"
#include "libs/fd_safety.h"
#include "libs/fvwmsignal.h"
#include "libs/log.h"
#include "Module.h"
#include "Parse.h"

/*
 * Loop until count bytes are read, unless an error or end-of-file
 * condition occurs.
 */
inline
static int positive_read(int fd, char *buf, int count)
{
	int requested = count;

	while (count > 0)
	{
		ssize_t n_read = fvwmRead(fd, buf, count);
		if (n_read < 0 && errno == EINTR && !isTerminated)
		{
			continue;
		}
		if (n_read <= 0)
		{
			if (n_read == 0)
			{
				errno = count == requested ? 0 : EPROTO;
			}
			return -1;
		}
		buf += n_read;
		count -= n_read;
	}
	return 0;
}


/*
 * Reads a single packet of info from fvwm.
 * The packet is stored in static memory that is reused during
 * the next call.
 */

FvwmPacket *ReadFvwmPacket(int fd)
{
	static unsigned long buffer[FvwmPacketMaxSize];
	FvwmPacket *packet = (FvwmPacket *)buffer;
	unsigned long length;

	/* Pipes preserve framing. Searching arbitrary words after corruption
	 * can wait indefinitely or mistake a body word for a new header. */
	if (positive_read(fd, (char *)buffer, sizeof(unsigned long)) < 0)
	{
		return NULL;
	}
	if (packet->start_pattern != START_FLAG)
	{
		errno = EPROTO;
		return NULL;
	}

	/* Now read the rest of the header */
	if (positive_read(fd, (char *)(&buffer[1]), 3 * sizeof(unsigned long))
		< 0)
	{
		if (errno == 0)
			errno = EPROTO;
		return NULL;
	}
	if (packet->size < FvwmPacketHeaderSize ||
	    packet->size > FvwmPacketMaxSize)
	{
		errno = EPROTO;
		return NULL;
	}
	length = FvwmPacketBodySize_byte(*packet);
	if (length > FvwmPacketMaxSize_byte - FvwmPacketHeaderSize_byte)
	{
		/* packet too long */
		errno = EPROTO;
		return NULL;
	}
	/* Finally, read the body, and we're done */
	if (positive_read(fd, (char *)(&buffer[4]), length) < 0)
	{
		if (errno == 0)
			errno = EPROTO;
		return NULL;
	}

	return packet;
}


/*
 *
 * SendFinishedStartupNotification - informs fvwm that the module has
 * finished its startup procedures and is fully operational now.
 *
 */
void SendFinishedStartupNotification(int *fd)
{
	SendText(fd, ModuleFinishedStartupResponse, 0);
}

/*
 *
 * SendUnlockNotification - informs fvwm that the module has
 * finished it's procedures and fvwm may proceed.
 *
 */
void SendUnlockNotification(int *fd)
{
	SendText(fd, ModuleUnlockResponse, 0);
}

/*
 *
 * SendQuitNotification - informs fvwm that the module has
 * finished and may be killed.
 *
 */
static unsigned long ModuleContinue = 1;
void SendQuitNotification(int *fd)
{
	ModuleContinue = 0;
	SendText(fd, ModuleUnlockResponse, 0); /* unlock just in case */
}

/*
 *
 * SendText - Sends arbitrary text/command back to fvwm
 *
 */
static void send_text_bytes(
	int *fd, const char *message, size_t message_length, unsigned long window)
{
	unsigned char buffer[3 * sizeof(unsigned long) + MAX_MODULE_INPUT_TEXT_LEN];
	unsigned char *p = buffer;
	unsigned long len;

	if (message_length > MAX_MODULE_INPUT_TEXT_LEN)
	{
		fvwm_debug(__func__, "Module command exceeds %d bytes",
			MAX_MODULE_INPUT_TEXT_LEN);
		return;
	}
	len = message_length;

	/* Put the message in the buffer, and... */
	memcpy(p, &window, sizeof(window));
	p += sizeof(unsigned long);

	memcpy(p, &len, sizeof(len));
	p += sizeof(unsigned long);

	memcpy(p, message, len);
	p += len;

	memcpy(p, &ModuleContinue, sizeof(unsigned long));
	p += sizeof(unsigned long);

	/* Keep partial progress; another frame must never follow an incomplete one.
	 * Module pipes retain their blocking contract, but termination cancels the
	 * syscall through the shared signal wrapper. The endpoint is borrowed. */
	size_t length = p - buffer;
	size_t written = 0;

	while (written < length && !isTerminated)
	{
		ssize_t count = fvwmWrite(fd[0], buffer + written, length - written);

		if (count > 0)
		{
			written += count;
		}
		else if (count < 0 && errno == EINTR && !isTerminated)
		{
			continue;
		}
		else
		{
			break;
		}
	}
	if (written != length)
	{
		fvwm_debug(__func__, "Incomplete module command write");
		fvwmSetTerminate(0);
	}
}

void SendText(int *fd, const char *message, unsigned long window)
{
	if (message != NULL)
	{
		send_text_bytes(fd, message,
			strnlen(message, MAX_MODULE_INPUT_TEXT_LEN + 1), window);
	}
}

/*
 *
 * SendFvwmPipe - Sends message to fvwm:  The message is a comma-delimited
 * string separated into its component sections and sent one by one to fvwm.
 * It is discouraged to use this function with a "synchronous" module.
 * (Form FvwmIconMan)
 *
 */
void SendFvwmPipe(int *fd, const char *message, unsigned long window)
{
	const char *hold = message;
	const char *temp;

	if (message == NULL || isTerminated)
	{
		return;
	}
	/* Keep the established literal-comma splitting, including empty fields. */
	while (!isTerminated && (temp = strchr(hold, ',')) != NULL)
	{
		send_text_bytes(fd, hold, temp - hold, window);
		hold = temp + 1;
	}
	if (!isTerminated)
	{
		SendText(fd, hold, window);
	}
}

void SetMessageMask(int *fd, unsigned long mask)
{
	char set_mask_mesg[50];

	snprintf(set_mask_mesg, sizeof(set_mask_mesg), "SET_MASK %lu", mask);
	SendText(fd, set_mask_mesg, 0);
}

void SetSyncMask(int *fd, unsigned long mask)
{
	char set_syncmask_mesg[50];

	snprintf(set_syncmask_mesg, sizeof(set_syncmask_mesg),
		"SET_SYNC_MASK %lu", mask);
	SendText(fd, set_syncmask_mesg, 0);
}

void SetNoGrabMask(int *fd, unsigned long mask)
{
	char set_nograbmask_mesg[50];

	snprintf(set_nograbmask_mesg, sizeof(set_nograbmask_mesg),
		"SET_NOGRAB_MASK %lu", mask);
	SendText(fd, set_nograbmask_mesg, 0);
}

/*
 * Optional routine that sets the matching criteria for config lines
 * that should be sent to a module by way of the GetConfigLine function.
 *
 * If this routine is not called, all module config lines are sent.
 */
static int first_pass = 1;

void InitGetConfigLine(int *fd, char *match)
{
	const char prefix[] = "Send_ConfigInfo ";
	char buffer[MAX_MODULE_INPUT_TEXT_LEN + 1];
	size_t prefix_length = sizeof(prefix) - 1;
	size_t match_length;

	first_pass = 0;
	if (match == NULL)
	{
		/* A null match is used by consumers requesting common/all config. */
		match = "";
	}
	match_length = strnlen(match,
		MAX_MODULE_INPUT_TEXT_LEN - prefix_length + 1);
	if (match_length > MAX_MODULE_INPUT_TEXT_LEN - prefix_length)
	{
		fvwm_debug(__func__, "Configuration request exceeds %d bytes",
			MAX_MODULE_INPUT_TEXT_LEN);
		fvwmSetTerminate(0);
		return;
	}
	memcpy(buffer, prefix, prefix_length);
	memcpy(buffer + prefix_length, match, match_length + 1);
	SendText(fd, buffer, 0);
}


/*
 * Gets a module configuration line from fvwm. Returns NULL if there are
 * no more lines to be had. "line" is a pointer to a char *.
 *
 * Changed 10/19/98 by Dan Espen:
 *
 * - The "isspace"  call was referring to  memory  beyond the end of  the
 * input area.  This could have led to the creation of a core file. Added
 * "body_size" to keep it in bounds.
 */
void GetConfigLine(int *fd, char **tline)
{
	FvwmPacket *packet;
	size_t body_count;
	char *line;

	*tline = NULL;

	if (first_pass)
	{
		SendText(fd, "Send_ConfigInfo", 0);
		first_pass = 0;
	}

	do
	{
		packet = ReadFvwmPacket(fd[1]);
		if (packet == NULL || packet->type == M_END_CONFIG_INFO)
		{
			*tline = NULL;
			return;
		}
	} while (packet->type != M_CONFIG_INFO);

	body_count = FvwmPacketBodySize(*packet) * sizeof(unsigned long);
	/* The three context words are outside the text. Require a terminator
	 * in the declared payload before any consumer can inspect this view. */
	if (body_count <= 3 * sizeof(unsigned long))
	{
		fvwm_debug(__func__, "Configuration packet has no text payload");
		return;
	}
	body_count -= 3 * sizeof(unsigned long);
	line = (char *)&packet->body[3];
	if (memchr(line, '\0', body_count) == NULL)
	{
		fvwm_debug(__func__, "Configuration packet text is not terminated");
		return;
	}

	while (body_count > 0 && isspace((unsigned char)*line))
	{
		line++;
		--body_count;
	}
	*tline = line;
}


static int module_descriptor(const char *text)
{
	char *end;
	long descriptor;

	if (text[0] == '\0' || strspn(text, "0123456789") != strlen(text))
	{
		errno = EINVAL;
		return -1;
	}
	errno = 0;
	descriptor = strtol(text, &end, 10);
	if (errno != 0 || *end != '\0' || descriptor > INT_MAX)
	{
		errno = ERANGE;
		return -1;
	}
	return (int)descriptor;
}

ModuleArgs *ParseModuleArgs(int argc, char *argv[], int use_arg6_as_alias)
{
	static ModuleArgs ma;
	ModuleArgs parsed = {0};

	/* Need at least six arguments:
	   [0] name of executable
	   [1] file descriptor of module->fvwm pipe (write end)
	   [2] file descriptor of fvwm->module pipe (read end)
	   [3] pathname of last config file read (ignored, use Send_ConfigInfo)
	   [4] application window context
	   [5] window decoration context

	   Optionally (left column used if use_arg6_as_alias is true):
	   [6] alias       or  user argument 0
	   [7] user arg 0  or  user arg 1
	   ...
	*/
	if (argc < 6)
	{
		return NULL;
	}
	parsed.to_fvwm = module_descriptor(argv[1]);
	if (parsed.to_fvwm < 0)
		return NULL;
	/* ModuleListenOnly deliberately omits the incoming endpoint. */
	parsed.from_fvwm = strcmp(argv[2], "-1") == 0 ? -1 :
		module_descriptor(argv[2]);
	if (parsed.from_fvwm < 0 && strcmp(argv[2], "-1") != 0)
		return NULL;
	if (parsed.to_fvwm == parsed.from_fvwm)
	{
		errno = EINVAL;
		return NULL;
	}
	/* The host passes these endpoints through the initial module exec.
	 * Retain them here, but do not pass them to module subprocesses. On
	 * failure leave ownership with the caller and refuse startup. */
	if (!fvwm_fd_set_cloexec(parsed.to_fvwm, true) ||
	    (parsed.from_fvwm >= 0 &&
	    !fvwm_fd_set_cloexec(parsed.from_fvwm, true)))
		return NULL;

	/* Module name is (last component of) argv[0] or possibly an alias
	   passed on the command line. */
	if (use_arg6_as_alias && argc >= 7)
	{
		parsed.name = argv[6];
		parsed.user_argc = argc - 7;
		parsed.user_argv = &(argv[7]);
	}
	else
	{
		char *p = strrchr(argv[0], '/');
		if (p == NULL)
		{
			parsed.name = argv[0];
		}
		else
		{
			parsed.name = ++p;
		}
		parsed.user_argc = argc - 6;
		parsed.user_argv = &(argv[6]);
	}

	parsed.namelen = strlen(parsed.name);

	if (parsed.user_argc == 0)
	{
		parsed.user_argv = NULL;
	}

	/* Ignore argv[3] */

	/* These two are generated as long hex strings */
	parsed.window = strtoul(argv[4], NULL, 16);
	parsed.decoration = strtoul(argv[5], NULL, 16);

	ma = parsed;
	return &ma;
}

/* expands certain variables in a command to be sent by a module */
char *module_expand_action(
	Display *dpy, int screen , char *in_action, rectangle *r,
	char *forecolor, char *backcolor)
{
	char *variables[] =
	{
		"$",
		"fg",
		"bg",
		"left",
		"-left",
		"right",
		"-right",
		"top",
		"-top",
		"bottom",
		"-bottom",
		"width",
		"height",
		NULL
	};
	char *action = NULL;
	char *src;
	char *dest;
	char *string = NULL;
	char *rest;
	int64_t val = 0;
	int offset;
	int i;
	char *dest_org;
	Bool is_string;
	Bool is_value;
	Bool has_geom;
	Bool has_fg;
	Bool has_bg;
	rectangle tmpr = { 0, 0, 0, 0 };

	if (in_action == NULL)
	{
		return NULL;
	}
	has_geom = (r == NULL) ? False : True;
	has_fg = (forecolor == NULL) ? False : True;
	has_bg = (backcolor == NULL) ? False : True;
	if (r == NULL)
	{
		r = &tmpr;
	}
	/* create a temporary storage for expanding */
	action = fxmalloc(MAX_MODULE_INPUT_TEXT_LEN + 1);
	for (src = in_action, dest = action; *src != 0; src++)
	{
		/* A temporary '$' may occupy the terminator slot while a known
		 * variable is replaced. It must never become admitted output. */
		if (dest - action > MAX_MODULE_INPUT_TEXT_LEN)
		{
			goto too_long;
		}
		if (*src != '$')
		{
			if (dest - action == MAX_MODULE_INPUT_TEXT_LEN)
			{
				goto too_long;
			}
			*(dest++) = *src;
			continue;
		}
		/* it's a variable */
		dest_org = dest;
		is_string = False;
		is_value = False;
		*(dest++) = *(src++);
		i = GetTokenIndex(src, variables, -1, &rest);
		if (i == -1)
		{
			src--;
			continue;
		}
		switch (i)
		{
		case 0: /* $ */
			continue;
		case 1: /* fg */
			string = forecolor;
			is_string = has_fg;
			break;
		case 2: /* bg */
			if (backcolor == NULL)
			{
				continue;
			}
			string = backcolor;
			is_string = has_bg;
			break;
		case 3: /* left */
			val = r->x;
			is_value = has_geom;
			break;
		case 4: /* -left */
			val = (int64_t)DisplayWidth(dpy, screen) - r->x - 1;
			is_value = has_geom;
			break;
		case 5: /* right */
			val = (int64_t)r->x + r->width;
			is_value = has_geom;
			break;
		case 6: /* -right */
			val = (int64_t)DisplayWidth(dpy, screen) - r->x -
				r->width - 1;
			is_value = has_geom;
			break;
		case 7: /* top */
			val = r->y;
			is_value = has_geom;
			break;
		case 8: /* -top */
			val = (int64_t)DisplayHeight(dpy, screen) - r->y - 1;
			is_value = has_geom;
			break;
		case 9: /* bottom */
			val = (int64_t)r->y + r->height;
			is_value = has_geom;
			break;
		case 10: /* -bottom */
			val = (int64_t)DisplayHeight(dpy, screen) - r->y -
				r->height - 1;
			is_value = has_geom;
			break;
		case 11: /* width */
			val = r->width;
			is_value = has_geom;
			break;
		case 12: /* height */
			val = r->height;
			is_value = has_geom;
			break;
		default: /* unknown */
			src--;
			continue;
		} /* switch */
		if (is_value == False && is_string == False)
		{
			src--;
			continue;
		}
		dest = dest_org;
		src = --rest;
		if (is_value)
		{
			/* print the number into the string */
			size_t remaining = MAX_MODULE_INPUT_TEXT_LEN -
				(dest - action);
			offset = snprintf(dest, remaining + 1, "%" PRId64, val);
			if (offset < 0 || (size_t)offset > remaining)
			{
				goto too_long;
			}
			dest += offset;
		}
		else if (is_string)
		{
			size_t remaining = MAX_MODULE_INPUT_TEXT_LEN -
				(dest - action);
			size_t length = strnlen(string, remaining + 1);
			if (length > remaining)
			{
				goto too_long;
			}
			memcpy(dest, string, length);
			dest += length;
		}
	} /* for */
	if (dest - action > MAX_MODULE_INPUT_TEXT_LEN)
	{
		goto too_long;
	}
	*dest = 0;

	return action;

too_long:
	fvwm_debug(__func__, "Expanded action exceeds %d bytes",
		MAX_MODULE_INPUT_TEXT_LEN);
	free(action);
	return NULL;
}
