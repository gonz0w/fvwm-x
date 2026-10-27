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

#include "config.h"
#include "fvwm/module_records.h"
#include "libs/fd_safety.h"
#include <errno.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

/* for F_CMD_ARGS */
#include "fvwm/fvwm.h"
#include "execcontext.h"
/* end of for CMD_ARGS */

/*for debug message*/
#include "fvwm.h"
#include "misc.h"
/* end of for debug message */

/* for get_current_read_file */
#include "read.h"
/* for busy cursor */
#include "cursor.h"

/* for Scr global */
#include "screen.h"

#include "externs.h"

/* for module syncronous */
#include "libs/fvwmsignal.h"
#include "libs/log.h"
#include "libs/System.h"
#include "libs/envvar.h"
#include "libs/Parse.h"
#include "libs/Strings.h"
#include "libs/wild.h"
#include "libs/fvwmsignal.h"
#include "libs/FEvent.h"
#include "events.h"
#include "event_loop_policy.h"
#include "bindings.h"

/* for positive write */

#include "module_list.h"
#include "module_interface.h"
/*
 * Use POSIX behaviour if we can, otherwise use SysV instead
 * should this be here?
 */
#ifndef O_NONBLOCK
#  define O_NONBLOCK  O_NDELAY
#endif

#ifndef FORK_CREATES_CHILD
#define FORK_CREATES_CHILD 0
#endif
#ifndef REMOVE_EXECUTABLE_EXTENSION
#define REMOVE_EXECUTABLE_EXTENSION 0
#endif

#define MOD_NOGRABMASK(m) ((m)->xNoGrabMask)
#define MOD_SYNCMASK(m) ((m)->xSyncMask)

typedef struct
{
	unsigned long *data;
	int size;
	int done;
} mqueue_object_type;


extern int moduleTimeout;

/*
 * static functions
 */
static void KillModuleByName(char *name, char *alias);
static char *get_pipe_name(fmodule *module);
static void DeleteMessageQueueBuff(fmodule *module);
static Bool flush_message_queue(fmodule *module,
	fvwm_module_deadline *deadline, Bool synchronous);
static Bool set_nonblocking(int fd);
static void positive_write(fmodule *module, unsigned long *ptr, int size,
	Bool defer_config, Bool synchronize);

static inline void msg_mask_set(
	msg_masks_t *msg_mask, unsigned long m1, unsigned long m2);
static void set_message_mask(msg_masks_t *mask, unsigned long msg);

static Bool set_nonblocking(int fd)
{
	int flags;

	if (fd < 0)
	{
		return True;
	}
	flags = fcntl(fd, F_GETFL);
	if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		return False;
	}

	return True;
}


/* These arrays remain owned by the constructor until a successful fork. */
static void close_module_pipes(int outgoing[2], int incoming[2])
{
	for (unsigned i = 0; i < 2; i++)
	{
		if (outgoing[i] >= 0)
		{
			close(outgoing[i]);
			outgoing[i] = -1;
		}
		if (incoming[i] >= 0)
		{
			close(incoming[i]);
			incoming[i] = -1;
		}
	}
}

static fmodule *do_execute_module(
	F_CMD_ARGS, Bool desperate, Bool do_listen_only)
{
	int fvwm_to_app[2], app_to_fvwm[2];
	int val;
	size_t i, nargs = 0, args_capacity = 7;
	char *cptr = NULL;
	char **args = NULL;
	char *arg1 = NULL;
	char arg2[20];
	char arg3[20];
	char arg5[20];
	char arg6[20];
	char *token;
	extern char *ModulePath;
	Window win;
	FvwmWindow * const fw = exc->w.fw;
	fmodule *module;

	fvwm_to_app[0] = -1;
	fvwm_to_app[1] = -1;
	app_to_fvwm[1] = -1;
	app_to_fvwm[0] = -1;
	args = fxmalloc(args_capacity * sizeof(*args));
	if (action == NULL)
	{
		goto err_exit;
	}
	if (fw)
	{
		win = FW_W(fw);
	}
	else
	{
		win = None;
	}
	action = GetNextToken(action, &cptr);
	if (!cptr)
	{
		goto err_exit;
	}
	arg1 = searchPath(ModulePath, cptr, EXECUTABLE_EXTENSION, X_OK);
	if (arg1 == NULL)
	{
		/* If this function is called in 'desparate' mode this means
		 * fvwm is trying a module name as a last resort.  In this case
		 * the error message is inappropriate because it was most
		 * likely a typo in a command, not a module name. */
		if (!desperate)
		{
			fvwm_debug(__func__,
				   "No such module '%s' in ModulePath '%s'",
				   cptr, ModulePath);
		}
		goto err_exit;
	}
	if (REMOVE_EXECUTABLE_EXTENSION && EXECUTABLE_EXTENSION != NULL)
	{
		char *p;
		char *ext = EXECUTABLE_EXTENSION;

		p = arg1 + strlen(arg1) - strlen(ext);
		if (strcmp(p, ext) == 0)
		{
			*p = 0;
		}
	}

	/* I want one-ended pipes, so I open two two-ended pipes,
	 * and close one end of each. I need one ended pipes so that
	 * I can detect when the module crashes/malfunctions */
	if (do_listen_only == True)
	{
		fvwm_to_app[0] = -1;
		fvwm_to_app[1] = -1;
	}
	else if (!fvwm_pipe_cloexec(fvwm_to_app))
	{
		fvwm_debug(__func__, "Failed to open pipe");
		goto err_exit;
	}
	if (!fvwm_pipe_cloexec(app_to_fvwm))
	{
		fvwm_debug(__func__, "Failed to open pipe2");
		goto err_exit;
	}
	if (
		fvwm_to_app[0] >= fvwmlib_max_fd ||
		fvwm_to_app[1] >= fvwmlib_max_fd ||
		app_to_fvwm[0] >= fvwmlib_max_fd ||
		app_to_fvwm[1] >= fvwmlib_max_fd)
	{
		fvwm_debug(__func__, "too many open fds");
		goto err_exit;
	}

	/* all ok, create the space and fill up */
	module = module_create();

	MOD_NAME(module) = stripcpy(cptr);
	free(cptr);
	snprintf(arg2, sizeof(arg2), "%d", app_to_fvwm[1]);
	snprintf(arg3, sizeof(arg3), "%d", fvwm_to_app[0]);
	snprintf(arg5, sizeof(arg5), "%lx", (unsigned long)win);
	snprintf(arg6, sizeof(arg6), "%lx", (unsigned long)exc->w.wcontext);
	args[0] = arg1;
	args[1] = arg2;
	args[2] = arg3;
	args[3] = (char *)get_current_read_file();
	if (!args[3])
	{
		args[3] = "none";
	}
	args[4] = arg5;
	args[5] = arg6;
	for (nargs = 6; action = GetNextToken(action, &token), token; nargs++)
	{
		/* Reserve the next token and terminator without per-token growth. */
		if (nargs + 1 == args_capacity)
		{
			args_capacity = args_capacity <=
				SIZE_MAX / sizeof(*args) / 2 ?
				args_capacity * 2 : nargs + 2;
			args = fxrealloc(args, args_capacity, sizeof(*args));
		}
		args[nargs] = token;
		if (MOD_ALIAS(module) == NULL)
		{
			const char *ptr = skipModuleAliasToken(args[nargs]);

			if (ptr && *ptr == '\0')
			{
				MOD_ALIAS(module) = stripcpy(args[nargs]);
			}
		}
	}
	args[nargs] = NULL;

	/* Config replies must describe the child's inherited table, even if
	 * core logging closes/reopens before a delayed or repeated request. */
	module->xloggingFd = log_get_fd();
	/* Child failure must not run the core's inherited stdio/exit handlers. */
	val = fork();
	if (val > 0)
	{
		/* This fork remains running fvwm */
		/* close appropriate descriptors from each pipe so
		 * that fvwm will be able to tell when the app dies */
		close(app_to_fvwm[1]);
		/* dont't care that this may be -1 */
		close(fvwm_to_app[0]);

		/* add these pipes to fvwm's active pipe list */
		MOD_WRITEFD(module) = fvwm_to_app[1];
		MOD_READFD(module) = app_to_fvwm[0];
		/* Preserve legacy default delivery; source metadata requires an
		 * explicit extended SET_MASK rather than inheriting bit 11. */
		msg_mask_set(&MOD_PIPEMASK(module), DEFAULT_MASK,
			DEFAULT_MASK & ~MX_CONFIG_SOURCE);
		if (DoingCommandLine)
		{
			/* add to the list of command line modules */
			MOD_SET_CMDLINE(module, 1);
		}

		/* Incremental framing requires both directions to be nonblocking. */
		if (!set_nonblocking(MOD_WRITEFD(module)) ||
		    !set_nonblocking(MOD_READFD(module)))
		{
			fvwm_debug(__func__, "module nonblocking setup failed");
			module_destroy(module);
			module = NULL;
		}
		/* module struct is completed, insert into the list */
		if (module != NULL)
		{
			module_register(module);
		}

	}
	else if (val == 0)
	{
		/* this is the child */
		/* this fork execs the module */
		if (FORK_CREATES_CHILD)
		{
			/* dont't care that this may be -1 */
			close(fvwm_to_app[1]);
			close(app_to_fvwm[0]);
		}
		/* Pass module endpoints; log inheritance is deliberate too. */
		if (!fvwm_fd_set_cloexec(app_to_fvwm[1], false) ||
		    (fvwm_to_app[0] >= 0 &&
		    !fvwm_fd_set_cloexec(fvwm_to_app[0], false)))
		{
			_exit(1);
		}
		fvmm_deinstall_signals();
		if (!Pdefault)
		{
			char visualid[32];
			char colormap[32];

			snprintf(visualid, sizeof(visualid),
				"FVWM_VISUALID=%lx",
				XVisualIDFromVisual(Pvisual));
			flib_putenv("FVWM_VISUALID", visualid);
			snprintf(colormap, sizeof(colormap),
				"FVWM_COLORMAP=%lx", Pcmap);
			flib_putenv("FVWM_COLORMAP", colormap);
		}
		else
		{
			flib_unsetenv("FVWM_VISUALID");
			flib_unsetenv("FVWM_COLORMAP");
		}

		/* Why is this execvp??  We've already searched the module
		 * path! */
		execvp(arg1, args);
		fvwm_debug(__func__, "Execution of module failed: %s", arg1);
		perror("");
		close(app_to_fvwm[1]);
		if (fvwm_to_app[0] >= 0)
		{
			close(fvwm_to_app[0]);
		}
		_exit(1);
	}
	else
	{
		fvwm_debug(__func__, "Fork failed");
		module_destroy(module);
		module = NULL;
		close_module_pipes(fvwm_to_app, app_to_fvwm);
	}
	free(arg1);
	for (i = 6; i < nargs; i++)
	{
		if (args[i] != 0)
		{
			free(args[i]);
		}
	}
	free(args);

	return module;

  err_exit:
	if (arg1 != NULL)
	{
		free(arg1);
	}
	if (cptr != NULL)
	{
		free(cptr);
	}
	if (args != NULL)
	{
		free(args);
	}
	close_module_pipes(fvwm_to_app, app_to_fvwm);

	return NULL;
}

fmodule *executeModuleDesperate(F_CMD_ARGS)
{
	fmodule *m;

	m = do_execute_module(F_PASS_ARGS, True, False);

	return m;
}

/* void module_send(fmodule *module, unsigned long *ptr, int size) */
/* This used to be marked "fvwm_inline".  I removed this
   when I added the lockonsend logic.  The routine seems too big to
   want to inline.  dje 9/4/98 */
extern int myxgrabcount;                /* defined in libs/Grab.c */
extern char *ModuleUnlock;              /* defined in libs/Module.c */
/* The deadline belongs to this command frame, even when input recursively
 * enters another synchronous operation. Retirement remains on death row. */
typedef struct
{
	fmodule *module;
	fvwm_module_deadline *deadline;
	Bool retire_on_timeout;
} module_sync_wait;

static Bool module_wait_remaining(fmodule *module,
	fvwm_module_deadline *deadline, int *remaining_ms,
	Bool retire_on_timeout)
{
	int unused_ms;
	fvwm_module_deadline_status status;

	if (isTerminated)
	{
		return False;
	}
	status = fvwm_module_deadline_poll_remaining(
		deadline, events_monotonic_ns(),
		remaining_ms != NULL ? remaining_ms : &unused_ms);
	if (status != FVWM_MODULE_DEADLINE_READY)
	{
		fvwm_debug(__func__, "module %p synchronous %s %s",
			module, retire_on_timeout ? "handshake" : "startup",
			status == FVWM_MODULE_DEADLINE_EXPIRED ?
			"deadline expired" : "clock/deadline invalid");
		if (retire_on_timeout)
		{
			module_kill(module);
		}
		return False;
	}
	return True;
}

static Bool module_sync_remaining(fmodule *module,
	fvwm_module_deadline *deadline, int *remaining_ms)
{
	return module_wait_remaining(module, deadline, remaining_ms, True);
}

static Bool module_sync_receive_ready(void *context)
{
	module_sync_wait *wait = context;

	return !MOD_IS_RETIRED(wait->module) && module_wait_remaining(
		wait->module, wait->deadline, NULL, wait->retire_on_timeout);
}

static void positive_write(fmodule *module, unsigned long *ptr, int size,
	Bool defer_config, Bool synchronize)
{
	msg_masks_t mask;
	size_t queue_entry_size;

	if (ptr == NULL || size < FvwmPacketHeaderSize * sizeof(*ptr) ||
	    size > FvwmPacketMaxSize_byte)
	{
		return;
	}
	if (MOD_WRITEFD(module) == -1 || MOD_IS_RETIRED(module))
	{
		return;
	}
	/* Destruction cancels any unsent snapshot even if it is unsubscribed. */
	if (ptr[1] == M_DESTROY_WINDOW &&
	    size >= (FvwmPacketHeaderSize + 1) * sizeof(*ptr))
	{
		module_config_discard(module, (Window)ptr[FvwmPacketHeaderSize]);
	}
	if (!IS_MESSAGE_IN_MASK(&(MOD_PIPEMASK(module)), ptr[1]))
	{
		return;
	}

	/* a dirty hack to prevent FvwmAnimate triggering during Recapture */
	/* would be better to send RecaptureStart and RecaptureEnd messages. */
	/* If module is lock on send for iconify message and it's an
	 * iconify event and server grabbed, then return */
	mask.m1 = (MOD_NOGRABMASK(module).m1 & MOD_SYNCMASK(module).m1);
	mask.m2 = (MOD_NOGRABMASK(module).m2 & MOD_SYNCMASK(module).m2);
	if (IS_MESSAGE_IN_MASK(&mask, ptr[1]) && myxgrabcount != 0)
	{
		return;
	}

	if (defer_config && ptr[1] == M_CONFIGURE_WINDOW &&
	    (IS_MESSAGE_IN_MASK(&module->xSyncMask, ptr[1]) ||
	     module->xdeferredConfig != NULL) &&
	    module_config_defer(module, ptr, (size_t)size))
	{
		return;
	}

	/* DV: This was once the AddToMessageQueue function.  Since it was only
	 * called once, put it in here for better performance. */
	{
		mqueue_object_type *c;

		queue_entry_size = sizeof(*c) + (size_t)size;
		if (!module_output_queue_reserve(module, queue_entry_size))
		{
			fvwm_debug(__func__,
				"module %p exceeded the %u-byte output queue limit",
				module, MODULE_QUEUE_MAX_BYTES);
			module_kill(module);
			return;
		}
		c = (mqueue_object_type *)malloc(
			queue_entry_size);
		if (c == NULL)
		{
			module_output_queue_release(module, queue_entry_size);
			fvwm_debug(__func__,
				"Output storage unavailable; retiring module");
			module_kill(module);
			return;
		}
		c->size = size;
		c->done = 0;
		c->data = (unsigned long *)(c + 1);
		memcpy((void*)c->data, (const void*)ptr, size);
		if (!fqueue_try_add_at_end(&(MOD_PIPEQUEUE(module)), c))
		{
			module_output_queue_release(module, queue_entry_size);
			free(c);
			fvwm_debug(__func__,
				"Output queue unavailable; retiring module");
			module_kill(module);
			return;
		}
	}

	/* dje, from afterstep, for FvwmAnimate, allows modules to sync with
	 * fvwm. this is disabled when the server is grabbed, otherwise
	 * deadlocks happen. M_LOCKONSEND has been replaced by a separated
	 * mask which defines on which messages the fvwm-to-module
	 * communication need to be lock on send. olicha Nov 13, 1999 */
	/* migo (19-Aug-2000): removed !myxgrabcount to sync M_DESTROY_WINDOW
	 */
	/* dv (06-Jul-2002): added the !myxgrabcount again.  Deadlocks *do*
	 * happen without it.  There must be another way to fix
	 * M_DESTROY_WINDOW handling in FvwmEvent. */
	/*if (IS_MESSAGE_IN_MASK(&(MOD_SYNCMASK(module)), ptr[1]))*/
	if (
		synchronize && IS_MESSAGE_IN_MASK(
			&(MOD_SYNCMASK(module)), ptr[1]) && !myxgrabcount)
	{
		struct pollfd ready = { MOD_READFD(module), POLLIN, 0 };
		int timeout_ms;
		Bool done = False;
		fmodule_input *input;

		fvwm_module_deadline deadline;
		module_sync_wait wait = { module, &deadline, True };

		/* Input callbacks may retire the producer and run cleanup. Keep
		 * its record until this entire synchronous frame unwinds. */
		module->xinputRefs++;
		fvwm_module_deadline_init(&deadline, events_monotonic_ns(),
			moduleTimeout);
		if (!flush_message_queue(module, &deadline, True))
		{
			module->xinputRefs--;
			return;
		}
		while (!done && !MOD_IS_RETIRED(module))
		{
			int rc;

			do
			{
				if (!module_sync_remaining(
					module, &deadline, &timeout_ms))
				{
					goto sync_done;
				}
				rc = fvwmPoll(&ready, 1, timeout_ms);
			} while (rc < 0 && !isTerminated && errno == EINTR);

			if (!module_sync_remaining(module, &deadline, NULL))
			{
				break;
			}
			/* A clamped interval can end before the deadline. */
			if (rc == 0)
			{
				continue;
			}
			if (rc > 0 && !(ready.revents & POLLNVAL) &&
			    (ready.revents & (POLLIN | POLLHUP | POLLERR)))
			{
				fmodule_input_status status;

				status = module_receive_checked(module, &input,
					module_sync_receive_ready, &wait);
				if (!module_sync_remaining(module, &deadline, NULL))
				{
					module_input_discard(input);
					break;
				}
				if (status == FMODULE_INPUT_ERROR)
				{
					done = True;
				}
				else if (status == FMODULE_INPUT_READY &&
					module_input_expect(input, ModuleUnlockResponse))
				{
					module_input_discard(input);
					done = True;
				}
				else if (status == FMODULE_INPUT_READY)
				{
					/* Retain execution of a final continuation=0
					 * command, then stop the retired producer. */
					module_input_execute(input);
				}
			}
			else
			{
				char *name = get_pipe_name(module);

				fvwm_debug(__func__,
					"Failed synchronous module read from '%s': rc=%d",
					name, rc);
				free(name);
				module_kill(module);
				break;
			}

			/* Execute input inline to preserve lock-on-send ordering.
			 * Recursion gets its own frame; the parent's deadline
			 * continues to run while nested commands execute. */
		}
	sync_done:
		module->xinputRefs--;
	}

	return;
}

void PositiveWrite(fmodule *module, unsigned long *ptr, int size)
{
	positive_write(module, ptr, size, True, True);
}

/* Repair publication at the outer boundary without re-entering commands. */
void module_output_enqueue(fmodule *module, unsigned long *packet, int size)
{
	positive_write(module, packet, size, True, False);
}

/* returns true if the module command matches "expect", false otherwise */
Bool module_input_expect(fmodule_input *input, char *expect)
{
	if (input == NULL || input->command == NULL || expect == NULL)
	{
		return False;
	}

	if (strncasecmp(input->command, expect, strlen(expect))	== 0)
	{
		/* the module sent the expected string */
		return True;
	}

	return False;
}

static void KillModuleByName(char *name, char *alias)
{
	fmodule_list_itr moditr;
	fmodule *module;

	if (name == NULL)
	{
		return;
	}
	module_list_itr_init(&moditr);
	while ( (module = module_list_itr_next(&moditr)) != NULL)
	{
		if (
			MOD_NAME(module) != NULL &&
			matchWildcards(name, MOD_NAME(module)) &&
			(!alias || (
				 MOD_ALIAS(module) &&
				 matchWildcards(alias, MOD_ALIAS(module)))))
		{
			module_kill(module);
		}
	}

	return;
}

static char *get_pipe_name(fmodule *module)
{
	char *name;

	if (MOD_NAME(module) != NULL)
	{
		if (MOD_ALIAS(module) != NULL)
		{
			xasprintf(&name, "%s %s", MOD_NAME(module),
			    MOD_ALIAS(module));
		}
		else
		{
			xasprintf(&name, "%s", MOD_NAME(module));
		}
	}
	else
	{
		name = fxstrdup("(null)");
	}

	return name;
}

/*
 * returns a pointer inside a string (just after the alias) if ok or NULL
 */
char *skipModuleAliasToken(const char *string)
{
#define is_valid_first_alias_char(ch) (isalpha(ch) || (ch) == '/')
#define is_valid_alias_char(ch) (is_valid_first_alias_char(ch)  \
				|| isalnum(ch) || (ch) == '-' || \
				(ch) == '.' || (ch) == '/')

	if (is_valid_first_alias_char(*string))
	{
		int len = 1;
		string++;
		while (*string && is_valid_alias_char(*string))
		{
			if (++len > MAX_MODULE_ALIAS_LEN)
			{
				return NULL;
			}
			string++;
		}
		return (char *)string;
	}

	return NULL;
#undef is_valid_first_alias_char
#undef is_valid_alias_char
}

/* message mask handling - does this belong here? */

static inline void msg_mask_set(
	msg_masks_t *msg_mask, unsigned long m1, unsigned long m2)
{
	msg_mask->m1 = m1;
	msg_mask->m2 = m2;

	return;
}

/*
 * Sets the mask to the specific value.  If M_EXTENDED_MSG is set in mask, the
 * function operates only on the extended messages, otherwise it operates only
 * on normal messages.
 */
static void set_message_mask(msg_masks_t *mask, unsigned long msg)
{
	if (msg & M_EXTENDED_MSG)
	{
		mask->m2 = (msg & ~M_EXTENDED_MSG);
	}
	else
	{
		mask->m1 = msg;
	}

	return;
}


/* message queues */

static void DeleteMessageQueueBuff(fmodule *module)
{
	mqueue_object_type *obj;

	if (fqueue_take_first(&(MOD_PIPEQUEUE(module)), (void **)&obj) == 1)
	{
		module_output_queue_release(
			module, sizeof(*obj) + (size_t)obj->size);
		/* we don't need to free the obj->data here because it's in the
		 * same malloced block as the obj itself. */
		free(obj);
	}

	return;
}

static Bool flush_message_queue(fmodule *module,
	fvwm_module_deadline *deadline, Bool synchronous)
{
	mqueue_object_type *obj;
	char *dptr;
	int a;
	fvwm_output_budget budget;

	if (module == NULL)
	{
		return False;
	}
	fvwm_output_budget_init(&budget);

	while (fqueue_get_first(&(MOD_PIPEQUEUE(module)), (void **)&obj) == 1)
	{
		if (MOD_IS_RETIRED(module))
		{
			return False;
		}
		if (!synchronous && budget.packets == 0)
		{
			return False;
		}
		dptr = (char *)obj->data;
		while (obj->done < obj->size)
		{
			if (deadline != NULL &&
			    !module_wait_remaining(module, deadline, NULL,
				synchronous))
			{
				return False;
			}
			size_t requested = (size_t)(obj->size - obj->done);

			/* Synchronous lock-on-send retains its completion barrier.
			 * Ready asynchronous readers yield with offsets intact. */
			if (!synchronous)
			{
				requested = fvwm_output_budget_request(
					&budget, requested);
				if (requested == 0)
				{
					return False;
				}
			}
			a = write(MOD_WRITEFD(module), &dptr[obj->done],
				  requested);
			if (a > 0)
			{
				obj->done += a;
				fvwm_output_budget_written(&budget, (size_t)a);
			}
			else if (a == 0)
			{
				module_kill(module);
				return False;
			}
			/* the write returns EWOULDBLOCK or EAGAIN if the pipe
			 * is full. (This is non-blocking I/O). SunOS returns
			 * EWOULDBLOCK, OSF/1 returns EAGAIN under these
			 * conditions. Hopefully other OSes return one of these
			 * values too. Solaris 2 doesn't seem to have a man
			 * page for write(2) (!) */
			else if (errno == EWOULDBLOCK || errno == EAGAIN)
			{
				struct pollfd ready = {
					MOD_WRITEFD(module), POLLOUT, 0 };
				int timeout_ms;
				int rc = 0;

				if (!synchronous)
				{
					return False;
				}
				do
				{
					/* Wait until the pipe accepts further
					 * input */
					if (!module_sync_remaining(
						module, deadline, &timeout_ms))
					{
						return False;
					}
					rc = fvwmPoll(&ready, 1, timeout_ms);
					/* Keep the deadline on EINTR. */
				} while ((rc < 0) && !isTerminated &&
					 (errno == EINTR));

				if (!module_sync_remaining(
					module, deadline, NULL))
				{
					return False;
				}
				if (rc == 0)
				{
					continue;
				}
				if (rc <= 0 || !(ready.revents & POLLOUT) ||
				    (ready.revents &
				     (POLLNVAL | POLLERR | POLLHUP)))
				{
					char *name;

					name = get_pipe_name(module);
					/* Doh! Something has gone wrong - get
					 * rid of the offender! */
					fvwm_debug(__func__,
						   "Failed to write descriptor to"
						   " '%s':\n"
						   "- poll rc=%d\n"
						   "- terminate signal=%c\n",
						   name, rc, isTerminated ?
						   'Y' : 'N');
					module_kill(module);
					free(name);

					return False;
				}

				/* pipe accepts further input; continue */
				continue;
			}
			else if (errno != EINTR)
			{
				module_kill(module);

				return False;
			}
		}
		DeleteMessageQueueBuff(module);
		fvwm_output_budget_completed(&budget);
	}

	return True;
}

/* Only the outer event loop calls this after command resources and grabs
 * unwind. Detach before callbacks and pin the producer across retirement. */
void module_config_flush_one(void)
{
	fmodule_list_itr iterator;
	fmodule *module;
	fmodule_config_packet *snapshot;

	if (!module_config_pending())
	{
		return;
	}
	module_list_itr_init(&iterator);
	while ((module = module_list_itr_next(&iterator)) != NULL)
	{
		if (module->xdeferredConfig == NULL)
		{
			continue;
		}
		snapshot = module_config_take(module);
		module->xinputRefs++;
		positive_write(module, snapshot->data, (int)snapshot->size, False, True);
		module->xinputRefs--;
		free(snapshot);
		module_list_itr_finish(&iterator);
		module_cleanup();
		return;
	}
}

void FlushMessageQueue(fmodule *module)
{
	flush_message_queue(module, NULL, False);
}

void FlushAllMessageQueues(void)
{
	fmodule_list_itr moditr;
	fmodule *module;

	module_list_itr_init(&moditr);
	while ( (module = module_list_itr_next(&moditr)) != NULL)
	{
		FlushMessageQueue(module);
	}

	return;
}

/* empty, only here so that the signal handling initialization code is the
 * same for modules and fvwm  */
RETSIGTYPE DeadPipe(int sig)
{
	SIGNAL_RETURN;
}

void CMD_Module(F_CMD_ARGS)
{
	do_execute_module(F_PASS_ARGS, False, False);

	return;
}

void CMD_ModuleListenOnly(F_CMD_ARGS)
{
	do_execute_module(F_PASS_ARGS, False, True);

	return;
}

void CMD_KillModule(F_CMD_ARGS)
{
	char *name;
	char *alias = NULL;

	action = GetNextToken(action,&name);
	if (!name)
	{
		return;
	}

	GetNextToken(action, &alias);
	KillModuleByName(name, alias);
	free(name);
	if (alias)
	{
		free(alias);
	}
	return;
}

void CMD_ModuleSynchronous(F_CMD_ARGS)
{
	int sec = moduleTimeout;
	char *next;
	char *token;
	char *expect = fxstrdup(ModuleFinishedStartupResponse);
	fmodule *module = NULL;
	fvwm_module_deadline deadline;
	Bool done = False;
	Bool need_ungrab = False;
	char *escape = NULL;
	XEvent tmpevent;

	if (!action)
	{
		goto done;
	}
	token = PeekToken(action, &next);
	if (StrEquals(token, "expect"))
	{
		token = PeekToken(next, &next);
		if (token)
		{
			free(expect);
			expect = fxstrdup(token);
		}
		action = next;
		token = PeekToken(action, &next);
	}
	if (token && StrEquals(token, "timeout"))
	{
		if (GetIntegerArguments(next, &next, &sec, 1) > 0 && sec > 0)
		{
			action = next;
		}
		else
		{
			fvwm_debug(__func__, "illegal timeout");
			goto done;
		}
	}
	if (!action)
	{
		goto done;
	}
	module = do_execute_module(F_PASS_ARGS, False, False);
	if (module == NULL)
	{
		goto done;
	}

	/* Inline input can retire the producer and run nested cleanup. The
	 * startup frame borrows it until the command barrier unwinds. */
	module->xinputRefs++;
	fvwm_module_deadline_init(&deadline, events_monotonic_ns(), sec);
	if (Scr.BusyCursor & BUSY_MODULESYNCHRONOUS)
	{
		if (GrabEm(CRS_WAIT, GRAB_BUSY))
		{
			need_ungrab = True;
		}
	}

	while (!done && !MOD_IS_RETIRED(module) && !isTerminated)
	{
		int timeout_ms;
		int read_fd = MOD_READFD(module);
		int write_fd = MOD_WRITEFD(module);
		int x_fd = ConnectionNumber(dpy);
		struct pollfd ready[3] = {
			{ read_fd, POLLIN, 0 }, { x_fd, POLLIN, 0 },
			{ write_fd >= 0 &&
			  !FQUEUE_IS_EMPTY(&MOD_PIPEQUEUE(module)) ?
			  write_fd : -1, POLLOUT, 0 }
		};
		int num_fd;
		int poll_error;
		module_sync_wait wait = { module, &deadline, False };

		if (!module_wait_remaining(
			module, &deadline, &timeout_ms, False))
		{
			break;
		}
		/* X readiness wakes Escape promptly. The finite fallback also
		 * checks keys that Xlib already buffered; other events stay queued. */
		if (timeout_ms > 100)
		{
			timeout_ms = 100;
		}
		num_fd = fvwmPoll(ready, 3, timeout_ms);
		poll_error = errno;
		if (!module_wait_remaining(module, &deadline, NULL, False))
		{
			break;
		}
		if (num_fd < 0)
		{
			if (poll_error == EINTR)
			{
				continue;
			}
			fvwm_debug(__func__,
				"module %p startup poll failed: %s",
				module, strerror(poll_error));
			break;
		}
		if ((ready[0].revents | ready[1].revents |
		    ready[2].revents) & POLLNVAL)
		{
			fvwm_debug(__func__,
				"module %p startup descriptor invalid", module);
			break;
		}
		if (num_fd > 0 &&
		    (ready[0].revents & (POLLIN | POLLHUP | POLLERR)))
		{
			fmodule_input *input;
			fmodule_input_status status;

			status = module_receive_checked(module, &input,
				module_sync_receive_ready, &wait);
			/* The checked reader tests the deadline before committing a
			 * complete frame to this command. A declined handoff retains
			 * the frame for ordinary asynchronous reception. */
			if (status == FMODULE_INPUT_ERROR)
			{
				done = True;
			}
			else if (status == FMODULE_INPUT_READY &&
				module_input_expect(input, expect))
			{
				module_input_discard(input);
				done = True;
			}
			else if (status == FMODULE_INPUT_READY)
			{
				/* Execute a final continuation=0 input before stopping
				 * its retired producer, just as ordinary input does. */
				module_input_execute(input);
			}
		}
		if (!module_wait_remaining(module, &deadline, NULL, False))
		{
			break;
		}
		if (!MOD_IS_RETIRED(module) && write_fd >= 0 &&
		    (ready[2].revents & (POLLOUT | POLLERR | POLLHUP)))
		{
			/* Retain asynchronous budgets and offsets; test the startup
			 * deadline before retries without killing a live module. */
			flush_message_queue(module, &deadline, False);
		}
		if (!module_wait_remaining(module, &deadline, NULL, False))
		{
			break;
		}
		/* Check for "escape function" */
		if (FPending(dpy) &&
		    FCheckMaskEvent(dpy, KeyPressMask, &tmpevent))
		{
			int context;
			XClassHint *class;
			char *name;
			Window w;

			context = GetContext(
				NULL, exc->w.fw, &tmpevent, &w);
			if (exc->w.fw != NULL)
			{
				class = &(exc->w.fw->class);
				name = exc->w.fw->name.name;
			}
			else
			{
				class = NULL;
				name = NULL;
			}
			escape = CheckBinding(
				Scr.AllBindings,
				tmpevent.xkey.keycode, tmpevent.xkey.state,
				GetUnusedModifiers(), context, BIND_KEYPRESS,
				class, name);
			if (escape != NULL)
			{
				if (!strcasecmp(escape,"escapefunc"))
				{
					done = True;
				}
			}
		}
	}

	if (need_ungrab)
	{
		UngrabEm(GRAB_BUSY);
	}
	module->xinputRefs--;
done:
	free(expect);
}

/* mask handling - does this belong here? */

void CMD_set_mask(F_CMD_ARGS)
{
	unsigned long val;

	if (exc->m.module == NULL)
	{
		return;
	}
	if (!action || sscanf(action, "%lu", &val) != 1)
	{
		val = 0;
	}
	set_message_mask(&(MOD_PIPEMASK(exc->m.module)), (unsigned long)val);

	return;
}

void CMD_set_sync_mask(F_CMD_ARGS)
{
	unsigned long val;

	if (exc->m.module == NULL)
	{
		return;
	}
	if (!action || sscanf(action,"%lu",&val) != 1)
	{
		val = 0;
	}
	set_message_mask(&(MOD_SYNCMASK(exc->m.module)), (unsigned long)val);

	return;
}

void CMD_set_nograb_mask(F_CMD_ARGS)
{
	unsigned long val;

	if (exc->m.module == NULL)
	{
		return;
	}
	if (!action || sscanf(action,"%lu",&val) != 1)
	{
		val = 0;
	}
	set_message_mask(&(MOD_NOGRABMASK(exc->m.module)), (unsigned long)val);

	return;
}
