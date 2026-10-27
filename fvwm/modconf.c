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
 * code for processing module configuration commands
 *
 * Modification History
 *
 * Created 09/23/98 by Dan Espen:
 *
 * - Some of this logic used to reside in "read.c", preceeded by a
 * comment about whether it belonged there.  Time tells.
 *
 * - Added  logic to  execute module config  commands by  passing to  the
 * modules that want "active command pipes".
 *
 * ********************************************************************
 */

#include "config.h"
#include "fvwm/module_records.h"
#include <stdio.h>

#include "libs/fvwmlib.h"
#include "libs/Parse.h"
#include "libs/Strings.h"
#include "libs/wild.h"
#include "fvwm.h"
#include "externs.h"
#include "cursor.h"
#include "functions.h"
#include "bindings.h"
#include "misc.h"
#include "screen.h"
#include "module_list.h"
#include "module_interface.h"
#include "colorset.h"
#include "libs/monitor_types.h"
#include "libs/log.h"
#include "expand.h"


#define MODULE_CONFIG_DELIM ':'

struct moduleInfoList
{
	char *data;
	char *source;
	unsigned char alias_len;
	struct moduleInfoList *next;
	struct moduleInfoList *retired_next;
	Bool retired;
};

struct moduleInfoList *modlistroot = NULL;

/* Retained provenance cannot grow with an unbounded configuration list.
 * Exhaustion preserves config text and sends explicit unavailable metadata. */
#define MODULE_SOURCE_STORAGE_MAX (1024U * 1024U)
static size_t module_source_bytes;
static unsigned long module_source_key;
static unsigned int module_config_traversals;
static struct moduleInfoList *module_config_retired;

/* Synchronous M_CONFIG_INFO callbacks can destroy the current or a later
 * entry, or recursively replay the list. Unlink immediately, but retain its
 * data and original next link until every borrowing traversal has returned. */
static void free_module_config_entry(struct moduleInfoList *entry)
{
	if (entry->source != NULL)
	{
		module_source_bytes -= strlen(entry->source) + 1;
		free(entry->source);
	}
	free(entry->data);
	free(entry);
}

static void retire_module_config_entry(struct moduleInfoList *entry)
{
	entry->retired = True;
	if (module_config_traversals == 0)
	{
		free_module_config_entry(entry);
		return;
	}
	entry->retired_next = module_config_retired;
	module_config_retired = entry;
}

static void finish_module_config_traversal(void)
{
	if (--module_config_traversals == 0)
	{
		while (module_config_retired != NULL)
		{
			struct moduleInfoList *entry = module_config_retired;

			module_config_retired = entry->retired_next;
			free_module_config_entry(entry);
		}
	}
}


static struct moduleInfoList *AddToModList(
	char *tline, const exec_context_t *source_exc);
static void SendConfigToModule(
	fmodule *module, const struct moduleInfoList *entry, char *match,
	int match_len);


/*
 * ModuleConfig handles commands starting with "*".
 *
 * Each command is added to a list from which modules  request playback
 * thru "sendconfig".
 *
 * Some modules request that module config commands be sent to them
 * as the commands are entered.  Send to modules that want it.
 */
void ModuleConfig(char *action, const exec_context_t *exc)
{
	int end;
	fmodule_list_itr moditr;
	fmodule *module;
	struct moduleInfoList *new_entry;

	if (action[0] == '*' && action[1] == 0)
	{
		return;
	}
#if 0 /*!!!*/
fprintf(stderr, "%s: action '%s'\n", __func__, (action) ? action : "(nil)");
#endif
	end = strlen(action) - 1;
	if (action[end] == '\n')
		action[end] = '\0';
	/* save for config request */
	new_entry = AddToModList(action, exc);
	/* Pin the entry across all receivers, including synchronous callbacks. */
	module_config_traversals++;
	/* look at all possible pipes */
	module_list_itr_init(&moditr);
	while ( (module = module_list_itr_next(&moditr)) != NULL)
	{
		if (IS_MESSAGE_SELECTED(module, M_SENDCONFIG))
		{
			/* module wants config cmds */
			char *name = MOD_NAME(module);
			if (MOD_ALIAS(module))
			{
				name = MOD_ALIAS(module);
			}
			char *modstring;

			xasprintf(&modstring, "%s%s", "*", name);
			SendConfigToModule(
				module, new_entry, modstring, 0);
			free(modstring);
		}
	}
	finish_module_config_traversal();

	return;
}

static struct moduleInfoList *AddToModList(
	char *tline, const exec_context_t *source_exc)
{
	struct moduleInfoList *t, *prev, *this;
	char *rline = tline;
	char *alias_end = skipModuleAliasToken(tline + 1);
	const exec_context_t *exc;

	/* Find end of list */
	t = modlistroot;
	prev = NULL;

	while(t != NULL)
	{
		prev = t;
		t = t->next;
	}

	this = fxmalloc(sizeof(struct moduleInfoList));

	this->retired = False;
	this->retired_next = NULL;
	this->source = NULL;
	{
		const char *json = fvwm_config_origin_json(source_exc->source);
		size_t size = strlen(json) + 1;
		if (size <= MODULE_SOURCE_STORAGE_MAX - module_source_bytes)
		{
			this->source = fxstrdup(json);
			module_source_bytes += size;
		}
	}
	this->alias_len = 0;
	if (alias_end && alias_end[0] == MODULE_CONFIG_DELIM)
	{
		/* migo (01-Sep-2000): construct an old-style config line */
		char *conf_start = alias_end + 1;
#if 0 /*!!!*/
fprintf(stderr, "%s: conf_start '%s'\n", __func__, conf_start);
#endif
		while (isspace(*conf_start)) conf_start++;
		*alias_end = '\0';
#if 0 /*!!!*/
fprintf(stderr, "%s: alias '%s'\n", __func__, tline ? tline : "(nil)");
fprintf(stderr, "%s: conf_start '%s'\n", __func__, conf_start);
#endif

		xasprintf(&rline, "%s%s", tline, conf_start);
		*alias_end = MODULE_CONFIG_DELIM;
		this->alias_len = alias_end - tline;
	}

	exc = exc_create_null_context();
	this->data = expand_vars(rline, NULL, False, True, NULL, exc);
#if 0 /*!!!*/
fprintf(stderr, "%s: mldata '%s', mlaliaslen %d\n", __func__, this->data, this->alias_len);
#endif
	exc_destroy_context(exc);
	/* Free rline only if it is xasprintf'd memory (not pointing at tline
	 * anymore). If we free our tline argument it causes a crash in
	 * _execute_function. */
	if (rline != tline)
		free(rline);

	this->next = NULL;
	if(prev == NULL)
	{
		modlistroot = this;
	}
	else
	{
		prev->next = this;
	}

	return this;
}

/*
 * delete from module configuration
 */
void CMD_DestroyModuleConfig(F_CMD_ARGS)
{
	struct moduleInfoList *current, *next, *prev;
	char *info;   /* info to be deleted - may contain wildcards */
	char *mi;
	char *alias_end;
	int alias_len = 0;

	while (isspace(*action))
	{
		action++;
	}
	alias_end = skipModuleAliasToken(action);

	if (alias_end && alias_end[0] == MODULE_CONFIG_DELIM)
	{
		/* migo: construct an old-style config line */
		char *conf_start = alias_end + 1;
		while (isspace(*conf_start)) conf_start++;
		*alias_end = '\0';
		GetNextToken(conf_start, &conf_start);
		if (conf_start == NULL)
		{
			return;
		}
		char *aconf;

		xasprintf(&aconf, "%s%s", action, conf_start);
		info = stripcpy(aconf);
		*alias_end = MODULE_CONFIG_DELIM;
		/* +1 for a leading '*' */
		alias_len = alias_end - action + 1;
		free(conf_start);
		free(aconf);
	}
	else
	{
		GetNextToken(action, &info);
		if (info == NULL)
		{
			return;
		}
	}

	current = modlistroot;
	prev = NULL;

	while (current != NULL)
	{
		GetNextToken(current->data, &mi);
		next = current->next;
		if ((!alias_len || !current->alias_len ||
		     alias_len == current->alias_len) &&
		    matchWildcards(info, mi+1))
		{
			if( prev )
			{
				prev->next = next;
			}
			else
			{
				modlistroot = next;
			}
			retire_module_config_entry(current);
		}
		else
		{
			prev = current;
		}
		current = next;
		if (mi)
		{
			free(mi);
		}
	}
	free(info);

	return;
}

static void send_desktop_names(fmodule *module)
{
	DesktopsInfo *d;
	char *name;
	struct monitor	*m = monitor_get_current();

	for (d = m->Desktops->next; d != NULL; d = d->next)
	{
		if (d->name != NULL)
		{
			xasprintf(&name, "DesktopName %d %s", d->desk, d->name);
			SendName(module, M_CONFIG_INFO, 0, 0, 0, name);
			free(name);
		}
	}

	return;
}

static void send_desktop_geometry(fmodule *module)
{
	char msg[64];
	struct monitor	*m = monitor_get_current();

	snprintf(msg, sizeof(msg), "DesktopSize %d %d\n",
		m->virtual_scr.VxMax / monitor_get_all_widths() + 1,
		m->virtual_scr.VyMax / monitor_get_all_heights() + 1);
	SendName(module, M_CONFIG_INFO, 0, 0, 0, msg);

	return;
}

static void send_image_path(fmodule *module)
{
	char *msg;
	char *ImagePath = PictureGetImagePath();

	if (ImagePath && *ImagePath != 0)
	{
		xasprintf(&msg, "ImagePath %s\n", ImagePath);
		SendName(module, M_CONFIG_INFO, 0, 0, 0, msg);
		free(msg);
	}

	return;
}

static void send_colorsets(fmodule *module)
{
	int n;

	/* dump the colorsets (0 first as others copy it) */
	for (n = 0; n < ColorsetCount(); n++)
	{
		SendName(
			module, M_CONFIG_INFO, 0, 0, 0,
			DumpColorset(ColorsetIdAt(n), GetColorset(ColorsetIdAt(n))));
	}

	return;
}

static void send_click_time(fmodule *module)
{
	char msg[64];

	/* Dominik Vogt (8-Nov-1998): Scr.ClickTime patch to set ClickTime to
	 * 'not at all' during InitFunction and RestartFunction. */
	snprintf(msg, sizeof(msg), "ClickTime %d\n", (Scr.ClickTime < 0) ?
		-Scr.ClickTime : Scr.ClickTime);
	SendName(module, M_CONFIG_INFO, 0, 0, 0, msg);

	return;
}

static void send_move_threshold(fmodule *module)
{
	char msg[64];

	snprintf(msg, sizeof(msg), "MoveThreshold %d\n", Scr.MoveThreshold);
	SendName(module, M_CONFIG_INFO, 0, 0, 0, msg);

	return;
}

void send_ignore_modifiers(fmodule *module)
{
	char msg[64];

	snprintf(msg, sizeof(msg), "IgnoreModifiers %d\n", GetUnusedModifiers());
	SendName(module, M_CONFIG_INFO, 0, 0, 0, msg);

	return;
}

void
send_logging_fd(fmodule *module)
{
	char msg[64];

	snprintf(msg, sizeof(msg), "LoggingFD %d\n", module->xloggingFd);
	SendName(module, M_CONFIG_INFO, 0, 0, 0, msg);
}

void send_monitor_list(fmodule *module)
{
	BroadcastMonitorList(module);
}

void CMD_Send_ConfigInfo(F_CMD_ARGS)
{
	struct moduleInfoList *t;
	/* matching criteria for module cmds */
	char *match;
	/* get length once for efficiency */
	int match_len = 0;
	fmodule *mod = exc->m.module;

	if (mod == NULL)
	{
		fvwm_debug(__func__,
		    "Ignoring configuration request outside a module context");
		return;
	}

	send_monitor_list(mod);
	/* send ImagePath and ColorLimit first */
	send_image_path(mod);
	BroadcastDesktopConfiguration(mod);
	send_colorsets(mod);
	send_click_time(mod);
	send_move_threshold(mod);
	send_desktop_geometry(mod);
	send_logging_fd(mod);
	match = PeekToken(action, &action);
	if (match)
	{
		match_len = strlen(match);
	}
	module_config_traversals++;
	for (t = modlistroot; t != NULL; t = t->next)
	{
		SendConfigToModule(mod, t, match, match_len);
	}
	finish_module_config_traversal();
	send_desktop_names(mod);
	send_ignore_modifiers(mod);
	SendPacket(
		mod, M_END_CONFIG_INFO, (long)0, (long)0, (long)0, (long)0,
		(long)0, (long)0, (long)0, (long)0);

	return;
}

static void SendConfigToModule(
	fmodule *module, const struct moduleInfoList *entry, char *match,
	int match_len)
{
	if (entry->retired)
		return;
	if (match)
	{
		if (match_len == 0)
		{
			match_len = strlen(match);
		}
		if (entry->alias_len > 0 && entry->alias_len != match_len)
		{
			return;
		}
		/* migo: this should be strncmp not strncasecmp probably. */
		if (strncasecmp(entry->data, match, match_len) != 0)
		{
			return;
		}
	}
	if (IS_MESSAGE_SELECTED(module, MX_CONFIG_SOURCE) &&
	    IS_MESSAGE_SELECTED(module, M_CONFIG_INFO))
	{
		/* Native ABI correlation, unrelated to X window identities. */
		if (++module_source_key == 0)
			++module_source_key;
		send_config_source(module, module_source_key,
			entry->source ? entry->source :
			fvwm_config_origin_json(NULL), entry->data);
	}
	else
	{
		SendName(module, M_CONFIG_INFO, 0, 0, 0, entry->data);
	}

	return;
}
