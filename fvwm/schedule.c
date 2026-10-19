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

#include <stdio.h>
#include <err.h>
#include <time.h>

#include "schedule_policy.h"
#include "event_loop_policy.h"

#include "libs/log.h"
#include "libs/fvwmlib.h"
#include "libs/fvwmsignal.h"
#include "libs/fqueue.h"
#include "libs/charmap.h"
#include "libs/wcontext.h"
#include "libs/Parse.h"
#include "libs/FEvent.h"
#include "fvwm.h"
#include "externs.h"

#include "colorset.h"
#include "bindings.h"
#include "misc.h"
#include "cursor.h"
#include "functions.h"
#include "commands.h"
#include "screen.h"
#include "module_interface.h"

typedef struct
{
	int id;
	uint64_t deadline_ns;
	Window window;
	char *command;
	int period; /* in milliseconds */
} sq_object_type;

static int last_schedule_id = 0;
static int next_schedule_id = -1;
static fqueue sq = FQUEUE_INIT;
static unsigned int schedule_remaining = FVWM_SCHEDULE_COMMAND_BUDGET;

static uint64_t schedule_monotonic_ns(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
	{
		err(1, "Cannot read monotonic scheduler clock");
	}
	return (uint64_t)now.tv_sec * 1000000000ULL + now.tv_nsec;
}

static int cmp_object_time(void *object1, void *object2, void *args)
{
	sq_object_type *so1 = (sq_object_type *)object1;
	sq_object_type *so2 = (sq_object_type *)object2;

	return (so1->deadline_ns > so2->deadline_ns) -
		(so1->deadline_ns < so2->deadline_ns);
}

static int check_deschedule_obj_func(void *object, void *args)
{
	sq_object_type *obj = object;

	return (obj->id == *(int *)args);
}

static void destroy_obj_func(void *object)
{
	sq_object_type *obj = object;

	if (obj->command != NULL)
	{
		free(obj->command);
	}
	free(obj);

	return;
}

static void deschedule(int *pid)
{
	int id;

	if (FQUEUE_IS_EMPTY(&sq))
	{
		return;
	}
	/* get the job group id to deschedule */
	if (pid != NULL)
	{
		id = *pid;
	}
	else
	{
		id = last_schedule_id;
	}
	/* deschedule matching jobs */
	fqueue_remove_or_operate_all(
		&sq, check_deschedule_obj_func, NULL, destroy_obj_func,
		(void *)&id);

	return;
}

static void schedule(
	Window window, char *command, uint64_t deadline_ns, int *pid,
	int period)
{
	sq_object_type *new_obj;

	if (command == NULL || *command == 0)
	{
		return;
	}
	/* create the new object */
	new_obj = fxcalloc(1, sizeof(sq_object_type));
	new_obj->window = window;
	new_obj->command = fxstrdup(command);
	new_obj->deadline_ns = deadline_ns;
	new_obj->period = period; /* 0 if this is not a periodic command */
	/* set the job group id */
	if (pid != NULL)
	{
		new_obj->id = *pid;
	}
	else
	{
		new_obj->id = next_schedule_id;
		next_schedule_id--;
		if (next_schedule_id >= 0)
		{
			/* wrapped around */
			next_schedule_id = -1;
		}
	}
	last_schedule_id = new_obj->id;
	/* insert into schedule queue */
	fqueue_add_inside(&sq, new_obj, cmp_object_time, NULL);

	return;
}

static int check_execute_obj_func(void *object, void *args)
{
	sq_object_type *obj = object;
	uint64_t *now_ns = args;

	return !isTerminated && *now_ns >= obj->deadline_ns;
}

static void execute_obj_func(void *object, void *args)
{
	sq_object_type *obj = object;

	if (obj->command != NULL)
	{
		/* execute the command */
		const exec_context_t *exc;
		exec_context_changes_t ecc;
		exec_context_change_mask_t mask = ECC_TYPE | ECC_WCONTEXT;

		ecc.type = EXCT_SCHEDULE;
		ecc.w.wcontext = C_ROOT;
		if (XFindContext(
			    dpy, obj->window, FvwmContext,
			    (caddr_t *)&ecc.w.fw) != XCNOENT)
		{
			ecc.w.wcontext = C_WINDOW;
			mask |= ECC_FW;
		}
		exc = exc_create_context(&ecc, mask);
		execute_function(NULL, exc, obj->command, NULL, 0);
		exc_destroy_context(exc);
	}
	if (obj->period > 0)
	{
		/* This is a periodic function, so reschedule it. */
		sq_object_type *new_obj = fxmalloc(sizeof(sq_object_type));
		memcpy(new_obj, obj, sizeof(sq_object_type));
		obj->command = NULL; /* new_obj->command now points to cmd. */
		new_obj->deadline_ns = fvwm_schedule_deadline_ns(
			schedule_monotonic_ns(), new_obj->period);
		/* insert into schedule queue */
		fqueue_add_inside(&sq, new_obj, cmp_object_time, NULL);
	}
	destroy_obj_func(obj);

	return;
}

/* Nested command waits share this budget until descriptors have been polled.
 * The executing object is privately owned, so Deschedule or a recursive wait
 * cannot destroy it or execute it a second time. */
void squeue_execute(void)
{
	uint64_t current_time;

	if (FQUEUE_IS_EMPTY(&sq))
	{
		return;
	}
	current_time = schedule_monotonic_ns();
	fvwm_ready_queue_drain(
		&sq, &schedule_remaining, check_execute_obj_func,
		execute_obj_func, &current_time);
	XFlush(dpy);

	return;
}

void squeue_after_poll(void)
{
	schedule_remaining = FVWM_SCHEDULE_COMMAND_BUDGET;
}

static int check_interaction_obj(void *object, void *args)
{
	sq_object_type *obj = object;

	return check_execute_obj_func(object, args) &&
		interaction_command_ready(obj->command, NULL);
}

void squeue_execute_interaction(void)
{
	uint64_t now = schedule_monotonic_ns();
	unsigned int remaining = schedule_remaining;
	unsigned int initial;
	void *saved_event;

	if (remaining > FVWM_INTERACTION_COMMAND_BUDGET)
	{
		remaining = FVWM_INTERACTION_COMMAND_BUDGET;
	}
	initial = remaining;
	saved_event = fev_save_event();
	fvwm_ready_queue_drain(
		&sq, &remaining, check_interaction_obj, execute_obj_func, &now);
	schedule_remaining -= initial - remaining;
	fev_restore_event(saved_event);
}

int squeue_get_interaction_ms(void)
{
	sq_object_type *obj;

	/* Timer deadline order is unchanged. An unsafe head waits for release
	 * rather than forcing a busy zero-timeout poll during the drag. */
	if (!fqueue_get_first(&sq, (void **)&obj) ||
	    !interaction_command_ready(obj->command, NULL))
	{
		return -1;
	}
	return fvwm_schedule_timeout_ms(
		schedule_monotonic_ns(), obj->deadline_ns);
}

/* returns the time in milliseconds to wait before next queue command must be
 * executed or -1 if none is queued */
int squeue_get_next_ms(void)
{
	int ms;
	sq_object_type *obj;

	if (fqueue_get_first(&sq, (void **)&obj) == 0)
	{
		return -1;
	}
	ms = fvwm_schedule_timeout_ms(
		schedule_monotonic_ns(), obj->deadline_ns);

	return ms;
}

int squeue_get_next_id(void)
{
	return next_schedule_id;
}

int squeue_get_last_id(void)
{
	return last_schedule_id;
}

void CMD_Schedule(F_CMD_ARGS)
{
	Window xw;
	uint64_t deadline_ns;
	char *taction;
	char *token;
	char *next;
	int ms;
	int id;
	int *pid;
	int n;
	FvwmWindow * const fw = exc->w.fw;
	Bool is_periodic = False;

	token = PeekToken(action, &next);
	if (token && strcasecmp(token, "periodic") == 0)
	{
		is_periodic = True;
		action = next;
	}
	/* get the time to execute */
	n = GetIntegerArguments(action, &action, &ms, 1);
	if (n <= 0)
	{
		fvwm_debug(__func__,
			   "Requires time to schedule as argument");
		return;
	}
	if (ms < 0)
	{
		ms = 0;
	}
	deadline_ns = fvwm_schedule_deadline_ns(schedule_monotonic_ns(), ms);
	/* get the job group id to schedule */
	n = GetIntegerArgumentsAnyBase(action, &taction, &id, 1);
	if (n >= 1)
	{
		pid = &id;
		action = taction;
	}
	else
	{
		pid = NULL;
	}
	/* get the window to operate on */
	if (fw != NULL)
	{
		xw = FW_W(fw);
	}
	else
	{
		xw = None;
	}
	/* schedule the job */
	schedule(xw, action, deadline_ns, pid, (is_periodic == True ? ms : 0));

	return;
}

void CMD_Deschedule(F_CMD_ARGS)
{
	int id;
	int *pid;
	int n;

	/* get the job group id to deschedule */
	n = GetIntegerArgumentsAnyBase(action, &action, &id, 1);
	if (n <= 0)
	{
		/* none, use default */
		pid = NULL;
	}
	else
	{
		pid = &id;
	}
	/* deschedule matching jobs */
	deschedule(pid);

	return;
}
