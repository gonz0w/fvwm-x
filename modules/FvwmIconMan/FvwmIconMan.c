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

#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "FvwmIconMan.h"
#include "readconfig.h"
#include "x.h"
#include "xmanager.h"

#include "libs/fvwmsignal.h"
#include "libs/log.h"
#include "libs/Module.h"
#include "libs/FTips.h"
#include "libs/FEvent.h"
#include "libs/Parse.h"


char *MyName;
FlocaleWinString *FwinString;
int mods_unused = DEFAULT_MODS_UNUSED;

static RETSIGTYPE TerminateHandler(int);

char *copy_string(char **target, const char *src)
{
	int len;
	if (src == NULL)
	{
		len = 0;
		if (*target)
		{
			Free(*target);
		}
		return *target = NULL;
	}

	len = strlen(src);
	ConsoleDebug(CORE, "copy_string: 1: 0x%lx\n", (unsigned long)*target);

	if (*target)
	{
		Free(*target);
	}

	ConsoleDebug(CORE, "copy_string: 2\n");
	*target = fxmalloc((len + 1) * sizeof(char));
	strcpy(*target, src);
	ConsoleDebug(CORE, "copy_string: 3\n");
	return *target;
}

#ifdef TRACE_MEMUSE

long MemUsed = 0;

void Free(void *p)
{
	struct malloc_header *head = (struct malloc_header *)p;

	if (p != NULL)
	{
		head--;
		if (head->magic != MALLOC_MAGIC)
		{
			fvwm_debug(MyName,
				   "Corrupted memory found in Free\n");
			abort();
			return;
		}
		if (head->len > MemUsed)
		{
			fvwm_debug(MyName, "Free block too big\n");
			return;
		}
		MemUsed -= head->len;
		Free(head);
	}
}

void PrintMemuse(void)
{
	ConsoleDebug(CORE, "Memory used: %ld\n", MemUsed);
}

#else

void Free(void *p)
{
	if (p != NULL)
	{
		free(p);
	}
}

void PrintMemuse(void)
{
}

#endif


static RETSIGTYPE
TerminateHandler(int sig)
{
	fvwmSetTerminate(sig);
	SIGNAL_RETURN;
}


void
ShutMeDown(int flag)
{
	ConsoleDebug(CORE, "Bye Bye\n");
	exit(flag);
}

void
DeadPipe(int nothing)
{
	(void)nothing;
	ShutMeDown(0);
	SIGNAL_RETURN;
}

static void
main_loop(module_packet_reader *reader)
{
	struct pollfd ready[2] = {
		{ x_fd, POLLIN, 0 }, { fvwm_fd[1], POLLIN, 0 }
	};
	int timeout_ms = 60000;

	while (!isTerminated)
	{
		unsigned long ms;
		int result;

		/* Queued X work keeps priority without waiting for new bytes. */
		result = fvwmPoll(ready, 2,
			FPending(theDisplay) ? 0 : timeout_ms);
		if (result < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			ConsoleMessage("Internal error with poll: errno=%s\n",
				strerror(errno));
			fvwmSetTerminate(0);
			break;
		}
		if (isTerminated)
		{
			break;
		}
		if (ready[0].revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			fvwmSetTerminate(0);
			break;
		}
		if ((ready[0].revents & POLLIN) || FPending(theDisplay))
		{
			xevent_loop();
		}
		if (!isTerminated &&
			(ready[1].revents & (POLLIN | POLLHUP)))
		{
			ReadFvwmPipe(reader);
		}
		if (ready[1].revents & (POLLERR | POLLNVAL))
		{
			fvwmSetTerminate(0);
		}
		if (isTerminated)
		{
			break;
		}
		ms = FTipsCheck(theDisplay);
		/* A large configured delay must never become an infinite poll. */
		timeout_ms = ms == 0 ? 60000 :
			(ms > INT_MAX ? INT_MAX : (int)ms);
	}
}

int
main(int argc, char **argv)
{
	int i;
	module_packet_reader reader;

	FlocaleInit(LC_CTYPE, "", "", "FvwmIconMan");

	OpenConsole(OUTPUT_FILE);

	init_globals();
	init_winlists();

	MyName = GetFileNameFromPath(argv[0]);

	if (argc >= 7)
	{
		if (strcasecmp(argv[6], "-Transient") == 0)
		{
			globals.transient = 1;

			/* Optionally accept an alias to use as the transient
			 * FvwmIconMan instance.
			 */
			if (argv[7])
				MyName = argv[7];
		}
		else
		{
			MyName = argv[6];
		}
	}
	ModuleLen = strlen(MyName) + 1;
	Module = fxmalloc(ModuleLen+1);
	*Module = '*';
	strcpy(Module+1, MyName);

	if (argc < 6)
	{
		fvwm_debug(MyName,
                           "%s version %s should only be executed by fvwm!\n",
                           __func__, VERSION);
		ShutMeDown(1);
	}
	ModuleArgs *module = ParseModuleArgs(argc, argv, 0);
	if (module == NULL)
	{
		fprintf(stderr, "%s: invalid module descriptors\n", argv[0]);
		return 1;
	}
	fvwm_fd[0] = module->to_fvwm;
	fvwm_fd[1] = module->from_fvwm;
	init_display();

	{
		struct sigaction  sigact;

		sigemptyset(&sigact.sa_mask);
		sigaddset(&sigact.sa_mask, SIGPIPE);
		sigaddset(&sigact.sa_mask, SIGINT);
		sigaddset(&sigact.sa_mask, SIGHUP);
		sigaddset(&sigact.sa_mask, SIGTERM);
		sigaddset(&sigact.sa_mask, SIGQUIT);
# ifdef SA_RESTART
		sigact.sa_flags = SA_RESTART;
# else
		sigact.sa_flags = 0;
# endif
		sigact.sa_handler = TerminateHandler;

		sigaction(SIGPIPE, &sigact, NULL);
		sigaction(SIGINT,  &sigact, NULL);
		sigaction(SIGHUP,  &sigact, NULL);
		sigaction(SIGTERM, &sigact, NULL);
		sigaction(SIGQUIT, &sigact, NULL);
	}

	read_in_resources();
	FlocaleAllocateWinString(&FwinString);

	for (i = 0; i < globals.num_managers; i++)
	{
		X_init_manager(i);
	}

	assert(globals.managers);

	SetMessageMask(
		fvwm_fd, M_CONFIGURE_WINDOW | M_RES_CLASS | M_RES_NAME |
		M_ADD_WINDOW | M_DESTROY_WINDOW | M_ICON_NAME |
		M_DEICONIFY | M_ICONIFY | M_ICON_LOCATION | M_END_WINDOWLIST |
		M_NEW_DESK | M_NEW_PAGE | M_FOCUS_CHANGE | M_WINDOW_NAME |
		M_CONFIG_INFO | M_SENDCONFIG | M_VISIBLE_NAME |
		M_MINI_ICON | M_STRING | M_WINDOWSHADE | M_DEWINDOWSHADE);
  /* extended messages */
  SetMessageMask(fvwm_fd, MX_VISIBLE_ICON_NAME | MX_PROPERTY_CHANGE);

  SendText(fvwm_fd, "Send_WindowList", 0);

  /* tell fvwm we're running */
  SendFinishedStartupNotification(fvwm_fd);

  /* Lock on send only for iconify and deiconify (for NoIconAction) */
  SetSyncMask(fvwm_fd, M_DEICONIFY | M_ICONIFY);
  SetNoGrabMask(fvwm_fd, M_DEICONIFY | M_ICONIFY);

	/* Blocking initial resources are complete before runtime reads. */
	if (!module_packet_reader_init(&reader, fvwm_fd[1]))
	{
		fvwm_debug(__func__, "cannot initialize runtime packet reader");
		return 1;
	}
	main_loop(&reader);

  return 0;
}
