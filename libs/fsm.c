/* -*-c-*- */
/* Copyright (C) 2003  Olivier Chapuis
 * some code taken from the xsm: Copyright 1993, 1998  The Open Group */

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

/* A set of functions for implementing a dummy sm. The code is based on xsm */

/* ---------------------------- included header files ---------------------- */

#include "config.h"
#include "libs/FProperty.h"
#include "fd_safety.h"
#include <stdio.h>
#include <errno.h>
#include <sys/wait.h>
#ifdef HAVE_GETPWUID
#include <pwd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <X11/Xtrans/Xtrans.h>


#include "fvwmlib.h"
#include "log.h"
#include "System.h"
#include "flist.h"
#include "fsm.h"
#include "ice_error_policy.h"

/* #define FVWM_DEBUG_FSM */

/* ---------------------------- local definitions -------------------------- */

/* ---------------------------- local macros ------------------------------- */

/* ---------------------------- imports ------------------------------------ */

/* ---------------------------- included code files ------------------------ */

/* ---------------------------- local types -------------------------------- */

typedef struct
{
	FIceConn		ice_conn;
	int		fd;
} fsm_ice_conn_t;

/* ---------------------------- forward declarations ----------------------- */

/* ---------------------------- local variables ---------------------------- */

static FIceAuthDataEntry *sauthDataEntries = NULL;
static FIceIOErrorHandler prev_handler;
static FIceListenObj *slistenObjs;

static char *addAuthFile = NULL;
static char *remAuthFile = NULL;

static flist *fsm_ice_conn_list = NULL;
static flist *pending_ice_conn_list = NULL;
static int numTransports = 0;
static char *networkIds = NULL;
static int *ice_fd = NULL;

static char *module_name = NULL;
static flist *running_list = NULL;
static Bool fsm_init_succeed = False;

static Atom _XA_WM_CLIENT_LEADER = None;
static Atom _XA_SM_CLIENT_ID = None;

/* ---------------------------- exported variables (globals) --------------- */

/* ---------------------------- local functions ---------------------------- */

/*
 * client list stuff
 */

static
void free_fsm_client(fsm_client_t *client)
{
	if (!SessionSupport)
	{
		return;
	}

	if (client->clientId)
	{
		free(client->clientId);
	}
	free(client);
}

/*
 * auth stuff
 */
static void
fprintfhex(register FILE *fp, unsigned int len, char *cp)
{
    static const char hexchars[] = "0123456789abcdef";

    for (; len > 0; len--, cp++) {
        unsigned char s = *cp;
        putc(hexchars[s >> 4], fp);
        putc(hexchars[s & 0x0f], fp);
    }
}

/*
 * Host Based Authentication Callback.  This callback is invoked if
 * the connecting client can't offer any authentication methods that
 * we can accept.  We can accept/reject based on the hostname.
 */

static
Bool HostBasedAuthProc(char *hostname)
{
    return (0);	      /* For now, we don't support host based authentication */
}

/*
 * We use temporary files which contain commands to add/remove entries from
 * the .ICEauthority file.
 */

static void
write_iceauth(FILE *addfp, FILE *removefp, FIceAuthDataEntry *entry)
{
	if (!SessionSupport)
	{
		return;
	}

	fprintf(addfp,
		"add %s \"\" %s %s ", entry->protocol_name, entry->network_id,
		entry->auth_name);
	fprintfhex(addfp, entry->auth_data_length, entry->auth_data);
	fprintf(addfp, "\n");

	fprintf(
		removefp,
		"remove protoname=%s protodata=\"\" netid=%s authname=%s\n",
		entry->protocol_name, entry->network_id, entry->auth_name);
}


static char *
unique_filename(char *path, char *prefix, int *pFd)
{
	char *tempFile;

	xasprintf(&tempFile, "%s/%sXXXXXX", path, prefix);
	*pFd =  fvwm_mkstemp(tempFile);
	if (*pFd == -1)
	{
		free(tempFile);
		tempFile = NULL;
	}
	return tempFile;
}

/* Invoke iceauth with a literal pathname: session save directories can contain
 * spaces or shell metacharacters. This helper runs only in isolated modules. */
static Bool
run_iceauth(const char *path)
{
	pid_t child = fork();
	int status;

	if (child < 0)
	{
		return False;
	}
	if (child == 0)
	{
		execlp("iceauth", "iceauth", "source", path, (char *)NULL);
		_exit(127);
	}
	pid_t result;
	do
	{
		result = waitpid(child, &status, 0);
	} while (result < 0 && errno == EINTR);
	return result == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void
free_auth_entries(int count, FIceAuthDataEntry *entries)
{
	if (entries == NULL)
	{
		return;
	}
	for (int i = 0; i < count * 2; i++)
	{
		free(entries[i].network_id);
		free(entries[i].auth_data);
	}
	free(entries);
}

/*
 * Provide authentication data to clients that wish to connect
 */

#define MAGIC_COOKIE_LEN 16

static
Status SetAuthentication(
	int count,FIceListenObj *listenObjs,FIceAuthDataEntry **authDataEntries)
{
	FILE *addfp = NULL;
	FILE *removefp = NULL;
	char *path;
	int original_umask;
	int i;
	int fd;

	*authDataEntries = NULL;
	if (!SessionSupport)
	{
		return 0;
	}

	original_umask = umask (0077);	/* disallow non-owner access */

	path = (char *)getenv("SM_SAVE_DIR");
	if (!path)
	{
		path = getenv("HOME");
	}

#ifdef HAVE_GETPWUID
	if (!path)
	{
		struct passwd *pwd;

		pwd = getpwuid(getuid());
		if (pwd)
		{
			path = pwd->pw_dir;
		}
	}
#endif
	if (!path)
	{
		path = ".";
	}
	if ((addAuthFile = unique_filename (path, ".fsm-", &fd)) == NULL)
	{
		goto bad;
	}
	if (!fvwm_fd_set_cloexec(fd, true) ||
	    !(addfp = fdopen(fd, "wb")))
	{
		close(fd);
		goto bad;
	}
	if ((remAuthFile = unique_filename (path, ".fsm-", &fd)) == NULL)
	{
		goto bad;
	}
	if (!fvwm_fd_set_cloexec(fd, true) ||
	    !(removefp = fdopen(fd, "wb")))
	{
		close(fd);
		goto bad;
	}

	*authDataEntries = fxcalloc(count * 2, sizeof(FIceAuthDataEntry));

	for (i = 0; i < count * 2; i += 2)
	{
		(*authDataEntries)[i].network_id =
			FIceGetListenConnectionString(listenObjs[i/2]);
		(*authDataEntries)[i].protocol_name = "ICE";
		(*authDataEntries)[i].auth_name = "MIT-MAGIC-COOKIE-1";

		(*authDataEntries)[i].auth_data =
			FIceGenerateMagicCookie (MAGIC_COOKIE_LEN);
		(*authDataEntries)[i].auth_data_length = MAGIC_COOKIE_LEN;

		(*authDataEntries)[i+1].network_id =
			FIceGetListenConnectionString(listenObjs[i/2]);
		(*authDataEntries)[i+1].protocol_name = "XSMP";
		(*authDataEntries)[i+1].auth_name = "MIT-MAGIC-COOKIE-1";

		(*authDataEntries)[i+1].auth_data =
			FIceGenerateMagicCookie (MAGIC_COOKIE_LEN);
		(*authDataEntries)[i+1].auth_data_length = MAGIC_COOKIE_LEN;

		write_iceauth(addfp, removefp, &(*authDataEntries)[i]);
		write_iceauth(addfp, removefp, &(*authDataEntries)[i+1]);

		FIceSetPaAuthData(2, &(*authDataEntries)[i]);
		FIceSetHostBasedAuthProc(listenObjs[i/2], HostBasedAuthProc);
	}

	Bool written = !ferror(addfp) && !ferror(removefp);
	int add_status = fclose(addfp);
	int remove_status = fclose(removefp);

	umask (original_umask);
	addfp = removefp = NULL;
	if (!written || add_status != 0 || remove_status != 0)
	{
		fvwm_debug(__func__, "Cannot write session authentication files");
		goto bad;
	}
	if (!run_iceauth(addAuthFile))
	{
		fvwm_debug(__func__, "iceauth could not install session authentication");
		goto bad;
	}

	unlink (addAuthFile);

	return 1;

 bad:

	umask(original_umask);
	free_auth_entries(count, *authDataEntries);
	*authDataEntries = NULL;
	if (addfp)
	{
		fclose (addfp);
	}

	if (removefp)
	{
		fclose (removefp);
	}

	if (addAuthFile)
	{
		unlink (addAuthFile);
		free (addAuthFile);
		addAuthFile = NULL;
	}
	if (remAuthFile)
	{
		unlink (remAuthFile);
		free (remAuthFile);
		remAuthFile = NULL;
	}

	return 0;
}

static
void FreeAuthenticationData(int count, FIceAuthDataEntry **authDataEntries)
{
	if (!SessionSupport)
	{
		return;
	}
	free_auth_entries(count, *authDataEntries);
	*authDataEntries = NULL;
	if (remAuthFile != NULL)
	{
		if (!run_iceauth(remAuthFile))
		{
			fvwm_debug(__func__,
				"iceauth could not remove session authentication");
		}
		unlink(remAuthFile);
	}

	free (addAuthFile);
	free (remAuthFile);
	addAuthFile = NULL;
	remAuthFile = NULL;
}

/*
 *  ice stuff
 */

static
void MyIoErrorHandler(FIceConn ice_conn)
{
	if (!SessionSupport)
	{
		return;
	}

	if (prev_handler)
	{
		(*prev_handler) (ice_conn);
	}
}

static
void InstallIOErrorHandler(void)
{
	FIceIOErrorHandler default_handler, candidate;

	if (!SessionSupport)
	{
		return;
	}

	candidate = FIceSetIOErrorHandler(NULL);
	default_handler = FIceSetIOErrorHandler(MyIoErrorHandler);
	prev_handler = fvwm_ice_error_previous(
		MyIoErrorHandler, prev_handler, candidate, default_handler);
}

static void
CloseListeners(void)
{
	if (!SessionSupport)
	{
		return;
	}

	if (slistenObjs != NULL)
	{
		FIceFreeListenObjs(numTransports, slistenObjs);
		slistenObjs = NULL;
		numTransports = 0;
	}
}

static
void ice_watch_fd(
	FIceConn conn, FIcePointer client_data, Bool opening,
	FIcePointer *watch_data)
{
	fsm_ice_conn_t *fice_conn;

	if (!SessionSupport)
	{
		return;
	}

	if (opening)
	{
		fice_conn = fxmalloc(sizeof(fsm_ice_conn_t));
		fice_conn->ice_conn = conn;
		fice_conn->fd = FIceConnectionNumber(conn);
		*watch_data = (FIcePointer) fice_conn;
		fsm_ice_conn_list =
			flist_append_obj(fsm_ice_conn_list, fice_conn);
	}
	else
	{
		fice_conn = (fsm_ice_conn_t *)*watch_data;
		/* IO errors can close a connection before protocol registration.
		 * Never leave its freed handle in the pending-status queue. */
		pending_ice_conn_list =
			flist_remove_obj(pending_ice_conn_list, conn);
		fsm_ice_conn_list =
			flist_remove_obj(fsm_ice_conn_list, fice_conn);
		free(fice_conn);
	}
}

/*
 * Session Manager callbacks
 */

static
Status RegisterClientProc(
	FSmsConn smsConn, FSmPointer managerData, char *previousId)
{
	fsm_client_t *client = (fsm_client_t *) managerData;
	char *id;
	int send_save = 0;

	if (!SessionSupport)
	{
		return 0;
	}
#ifdef FVWM_DEBUG_FSM
	fvwm_debug(__func__,
		   "[%s][RegisterClientProc] On FIceConn fd = %d, received "
		   "REGISTER CLIENT [Previous Id = %s]\n", module_name,
		   FIceConnectionNumber (client->ice_conn),
		   previousId ? previousId : "NULL");
#endif

	/* ignore previousID!! (we are dummy) */
	id = FSmsGenerateClientID(smsConn);
	if (!FSmsRegisterClientReply(smsConn, id))
	{
		/* cannot happen ? */
		if (id)
		{
			free(id);
		}
		fvwm_debug(__func__,
			   "[%s][RegisterClientProc] ERR -- fail to register "
			   "client", module_name);
		return 0;
	}

#ifdef FVWM_DEBUG_FSM
	fvwm_debug(__func__,
		   "[%s][RegisterClientProc] On FIceConn fd = %d, sent "
		   "REGISTER CLIENT REPLY [Client Id = %s]\n",
		   module_name, FIceConnectionNumber (client->ice_conn), id);
#endif

	client->clientId = id;
	/* client->clientHostname = FSmsClientHostName (smsConn); */

	/* we are dummy ... do not do that */
	if (send_save)
	{
		FSmsSaveYourself(
			smsConn, FSmSaveLocal, False, FSmInteractStyleNone,
			False);
	}

	return 1;
}

static void
InteractRequestProc(FSmsConn smsConn, FSmPointer managerData, int dialogType)
{
	if (!SessionSupport)
	{
		return;
	}
	/* no intercation! */
#if 0
	FSmsInteract (client->smsConn);
#endif
}


static void
InteractDoneProc(FSmsConn smsConn, FSmPointer managerData, Bool cancelShutdown)
{
	/* no intercation! */
}

static void
SaveYourselfReqProc(FSmsConn smsConn, FSmPointer managerData, int saveType,
    Bool shutdown, int interactStyle, Bool fast, Bool global)
{
	/* no session to save */
}

static
void SaveYourselfPhase2ReqProc(FSmsConn smsConn, FSmPointer managerData)
{
	fsm_client_t *client;

	if (!SessionSupport)
	{
		return;
	}
	client = (fsm_client_t *)managerData;
	SUPPRESS_UNUSED_VAR_WARNING(client);
	FSmsSaveYourselfPhase2(client->smsConn);
}

static void
SaveYourselfDoneProc(FSmsConn smsConn, FSmPointer managerData, Bool success)
{
	fsm_client_t	*client;

	if (!SessionSupport)
	{
		return;
	}
	client = (fsm_client_t *) managerData;
	SUPPRESS_UNUSED_VAR_WARNING(client);
	FSmsSaveComplete(client->smsConn);
}

static void
CloseDownClient(fsm_client_t *client)
{

	if (!SessionSupport)
	{
		return;
	}

#ifdef FVWM_DEBUG_FSM
	fvwm_debug(__func__, "[%s][CloseDownClient] ICE Connection closed, "
		   "FIceConn fd = %d\n", module_name,
		   FIceConnectionNumber (client->ice_conn));
#endif

	FSmsCleanUp(client->smsConn);
	FIceSetShutdownNegotiation(client->ice_conn, False);
	if ((!FIceCloseConnection(client->ice_conn)) != FIceClosedNow)
	{
		/* do not care */
	}

	client->ice_conn = NULL;
	client->smsConn = NULL;

	running_list = flist_remove_obj(running_list, client);
	free_fsm_client(client);
}

static void
CloseConnectionProc(FSmsConn smsConn, FSmPointer managerData,
		    int count, char **reasonMsgs)
{
    fsm_client_t	*client = (fsm_client_t *) managerData;

    if (!SessionSupport)
    {
	    return;
    }
    FSmFreeReasons(count, reasonMsgs);
    CloseDownClient(client);
}

static
void SetPropertiesProc(
	FSmsConn smsConn, FSmPointer managerData, int numProps, FSmProp **props)
{
	int i;

	if (!SessionSupport)
	{
		return;
	}
	for (i = 0; i < numProps; i++)
	{
		FSmFreeProperty(props[i]);
	}
	free ((char *) props);
}

static
void DeletePropertiesProc(
	FSmsConn smsConn, FSmPointer managerData, int numProps, char **propNames)
{
	int i;

	if (!SessionSupport)
	{
		return;
	}
	for (i = 0; i < numProps; i++)
	{
		free (propNames[i]);
	}
	free ((char *) propNames);
}

static
void GetPropertiesProc(FSmsConn smsConn, FSmPointer managerData)
{
}

static Status
NewClientProc(
	FSmsConn smsConn, FSmPointer managerData, unsigned long *maskRet,
	FSmsCallbacks *callbacksRet, char **failureReasonRet)
{
    fsm_client_t *nc;

    if (!SessionSupport)
    {
	    return 0;
    }

    nc = fxmalloc(sizeof (fsm_client_t));
    *maskRet = 0;

    nc->smsConn = smsConn;
    nc->ice_conn = FSmsGetIceConnection(smsConn);
    nc->clientId = NULL;

    running_list = flist_append_obj(running_list, nc);

#ifdef FVWM_DEBUG_FSM
	fvwm_debug(__func__,
                   "[%s][NewClientProc] On FIceConn fd = %d, client "
                   "set up session mngmt protocol\n", module_name,
                   FIceConnectionNumber (nc->ice_conn));
#endif

    /*
     * Set up session manager callbacks.
     */

    *maskRet |= FSmsRegisterClientProcMask;
    callbacksRet->register_client.callback 	= RegisterClientProc;
    callbacksRet->register_client.manager_data  = (FSmPointer) nc;

    *maskRet |= FSmsInteractRequestProcMask;
    callbacksRet->interact_request.callback 	= InteractRequestProc;
    callbacksRet->interact_request.manager_data = (FSmPointer) nc;

    *maskRet |= FSmsInteractDoneProcMask;
    callbacksRet->interact_done.callback	= InteractDoneProc;
    callbacksRet->interact_done.manager_data    = (FSmPointer) nc;

    *maskRet |= FSmsSaveYourselfRequestProcMask;
    callbacksRet->save_yourself_request.callback     = SaveYourselfReqProc;
    callbacksRet->save_yourself_request.manager_data = (FSmPointer) nc;

    *maskRet |= FSmsSaveYourselfP2RequestProcMask;
    callbacksRet->save_yourself_phase2_request.callback =
	SaveYourselfPhase2ReqProc;
    callbacksRet->save_yourself_phase2_request.manager_data =
	(FSmPointer) nc;

    *maskRet |= FSmsSaveYourselfDoneProcMask;
    callbacksRet->save_yourself_done.callback 	   = SaveYourselfDoneProc;
    callbacksRet->save_yourself_done.manager_data  = (FSmPointer) nc;

    *maskRet |= FSmsCloseConnectionProcMask;
    callbacksRet->close_connection.callback 	 = CloseConnectionProc;
    callbacksRet->close_connection.manager_data  = (FSmPointer) nc;

    *maskRet |= FSmsSetPropertiesProcMask;
    callbacksRet->set_properties.callback 	= SetPropertiesProc;
    callbacksRet->set_properties.manager_data   = (FSmPointer) nc;

    *maskRet |= FSmsDeletePropertiesProcMask;
    callbacksRet->delete_properties.callback	= DeletePropertiesProc;
    callbacksRet->delete_properties.manager_data   = (FSmPointer) nc;

    *maskRet |= FSmsGetPropertiesProcMask;
    callbacksRet->get_properties.callback	= GetPropertiesProc;
    callbacksRet->get_properties.manager_data   = (FSmPointer) nc;

    return 1;
}

/*
 *
 */

static
void CompletNewConnectionMsg(void)
{
	flist *l = pending_ice_conn_list;
	FIceConn ice_conn;
	FIceAcceptStatus cstatus;

	if (!SessionSupport)
	{
		return;
	}

	while(l != NULL)
	{
		ice_conn = (FIceConn)l->object;
		cstatus = (FIceAcceptStatus)FIceConnectionStatus(ice_conn);
		if (cstatus == (int)FIceConnectPending)
		{
			l = l->next;
		}
		else if (cstatus == (int)FIceConnectAccepted)
		{
			l = l->next;
			pending_ice_conn_list =
				flist_remove_obj(
					pending_ice_conn_list,
					ice_conn);
#ifdef FVWM_DEBUG_FSM
			char *connstr;

			connstr = FIceConnectionString (ice_conn);
			fvwm_debug(__func__,
				   "[%s][NewConnection] ICE Connection "
				   "opened by client, FIceConn fd = %d, Accept at "
				   "networkId %s\n", module_name,
				   FIceConnectionNumber (ice_conn), connstr);
			free (connstr);
#endif
		}
		else
		{
			l = l->next;
			if (FIceCloseConnection (ice_conn) != FIceClosedNow)
			{
				/* don't care */
			}
			pending_ice_conn_list =
				flist_remove_obj(
					pending_ice_conn_list,
					ice_conn);
#ifdef FVWM_DEBUG_FSM
			if (cstatus == FIceConnectIOError)
			{
				fvwm_debug(__func__,
					   "[%s][NewConnection] IO error "
					   "opening ICE Connection!\n",
					   module_name);
			}
			else
			{
				fvwm_debug(__func__,
					   "[%s][NewConnection] ICE "
					   "Connection rejected!\n",
					   module_name);
			}
#endif
		}
	}
}

static
void NewConnectionMsg(int i)
{
	FIceConn ice_conn;
	FIceAcceptStatus status;

	if (!SessionSupport)
	{
		return;
	}

	SUPPRESS_UNUSED_VAR_WARNING(status);
	ice_conn = FIceAcceptConnection(slistenObjs[i], &status);
#ifdef FVWM_DEBUG_FSM
	fvwm_debug(__func__, "[%s][NewConnection] %i\n", module_name, i);
#endif
	if (!ice_conn)
	{
#ifdef FVWM_DEBUG_FSM
		fvwm_debug(__func__, "[%s][NewConnection] "
			   "FIceAcceptConnection failed\n", module_name);
#endif
	}
	else
	{
		/* Do not close a connection inside its libICE watch callback. */
		if (!fvwm_fd_prepare_select(FIceConnectionNumber(ice_conn)))
		{
			fvwm_debug(__func__, "Cannot prepare accepted ICE descriptor");
			FIceCloseConnection(ice_conn);
			return;
		}
		pending_ice_conn_list =
			flist_append_obj(pending_ice_conn_list, ice_conn);

		CompletNewConnectionMsg();
	}
}

static
void ProcessIceMsg(fsm_ice_conn_t *fic)
{
	FIceProcessMessagesStatus status;
	FIceConn connection = fic->ice_conn;

	if (!SessionSupport)
	{
		return;
	}

#ifdef FVWM_DEBUG_FSM
	fvwm_debug(__func__, "[%s][ProcessIceMsg] %i\n", module_name,
		   (int)fic->fd);
#endif

	status = FIceProcessMessages(connection, NULL, NULL);

	if (status == FIceProcessMessagesIOError)
	{
		flist *cl;
		int found = 0;

#ifdef FVWM_DEBUG_FSM
		fvwm_debug(__func__,
			   "[%s][ProcessIceMsg] IO error on connection\n",
			   module_name);
#endif

		for (cl = running_list; cl; cl = cl->next)
		{
			fsm_client_t *client = (fsm_client_t *) cl->object;

			if (client->ice_conn == connection)
			{
				CloseDownClient (client);
				found = 1;
				break;
			}
		}

		if (!found)
		{
			/*
			 * The client must have disconnected before it was added
			 * to the session manager's running list (i.e. before the
			 * NewClientProc callback was invoked).
			 */

			FIceSetShutdownNegotiation (connection, False);
			if ((!FIceCloseConnection(connection)) != FIceClosedNow)
			{
				/* do not care */
			}
		}
	}
}

/*
 * proxy stuff
 */
static
char *GetClientID(Display *dpy, Window window)
{
	char *client_id = NULL;
	Window client_leader = None;

	if (!SessionSupport)
	{
		return NULL;
	}

	if (!_XA_WM_CLIENT_LEADER)
	{
		_XA_WM_CLIENT_LEADER = XInternAtom(
			dpy, "WM_CLIENT_LEADER", False);
	}
	if (!_XA_SM_CLIENT_ID)
	{
		_XA_SM_CLIENT_ID = XInternAtom(dpy, "SM_CLIENT_ID", False);
	}

	client_leader = FGetClientLeader(dpy, window, _XA_WM_CLIENT_LEADER);
	if (!client_leader)
	{
		client_leader = window;
	}
	client_id = FGetSessionString(dpy, client_leader, _XA_SM_CLIENT_ID);

	return client_id;
}

static
void set_session_manager(Display *dpy, Window window, char *sm)
{
	Window client_leader = None;
	char *dummy_id;
	static Atom _XA_SESSION_MANAGER = None;

	if (!SessionSupport)
	{
		return;
	}

	dummy_id = "0";

	if (!_XA_SESSION_MANAGER)
	{
		_XA_SESSION_MANAGER = XInternAtom (
			dpy, "SESSION_MANAGER", False);
	}
	if (!_XA_WM_CLIENT_LEADER)
	{
		_XA_WM_CLIENT_LEADER = XInternAtom(
			dpy, "WM_CLIENT_LEADER", False);
	}
	if (!_XA_SM_CLIENT_ID)
	{
		_XA_SM_CLIENT_ID = XInternAtom(dpy, "SM_CLIENT_ID", False);
	}

	client_leader = FGetClientLeader(dpy, window, _XA_WM_CLIENT_LEADER);

	if (!client_leader)
	{
		client_leader = window;
	}

#ifdef FVWM_DEBUG_FSM
	fvwm_debug(__func__, "[%s][fsm_init] Proxy %s window 0x%lx\n",
		   module_name, (sm)? "On":"Off", client_leader);

#endif

	/* set the client id for ksmserver */
	if (sm)
	{
		XChangeProperty(
			dpy, client_leader, _XA_SESSION_MANAGER, XA_STRING,
			8, PropModeReplace, (unsigned char *)sm, strlen(sm));
		XChangeProperty(
			dpy, client_leader, _XA_SM_CLIENT_ID, XA_STRING,
			8, PropModeReplace, (unsigned char *)dummy_id,
			strlen(dummy_id));
	}
	else
	{
		XDeleteProperty(dpy, client_leader, _XA_SESSION_MANAGER);
		XDeleteProperty(dpy, client_leader, _XA_SM_CLIENT_ID);
	}
}

/* ---------------------------- interface functions ------------------------ */

int fsm_init(char *module)
{
	char errormsg[256];
	int i;
	char *p;

	if (!SessionSupport)
	{
		/* -Wall fix */
		MyIoErrorHandler(NULL);
		HostBasedAuthProc(NULL);
		ice_watch_fd(0, NULL, 0, NULL);
		NewClientProc(0, NULL, 0, NULL, NULL);
#ifdef FVWM_DEBUG_FSM
		fvwm_debug(__func__, "[%s][fsm_init] No Session Support\n",
			   module_name);
#endif
		return 0;
	}

	if (fsm_init_succeed)
	{
		fvwm_debug(__func__, "[%s][fsm_init] <<ERROR>> -- "
			   "Session already Initialized!\n", module_name);
		return 1;
	}
	module_name = module;
	InstallIOErrorHandler ();

#ifdef FVWM_DEBUG_FSM
	fvwm_debug(__func__, "[%s][fsm_init] FSmsInitialize\n", module_name);
#endif
	if (!FSmsInitialize(
		    module, "1.0", NewClientProc, NULL, HostBasedAuthProc, 256,
		    errormsg))
	{
		fvwm_debug(__func__, "[%s][fsm_init] <<ERROR>> -- "
			   "FSmsInitialize failed: %s\n",
			   module_name, errormsg);
		return 0;
	}


#ifdef HAVE__ICETRANSNOLISTEN
	_IceTransNoListen("tcp");
#endif


	if (!FIceListenForConnections (
		    &numTransports, &slistenObjs, 256, errormsg))
	{
		fvwm_debug(__func__, "[%s][fsm_init] <<ERROR>> -- "
			   "FIceListenForConnections failed:\n"
			   "%s\n", module_name, errormsg);
		return 0;
	}

	for (i = 0; i < numTransports; i++)
	{
		if (!fvwm_fd_prepare_select(
			FIceGetListenConnectionNumber(slistenObjs[i])))
		{
			fvwm_debug(__func__, "Cannot prepare ICE listener descriptor");
			FIceFreeListenObjs(numTransports, slistenObjs);
			slistenObjs = NULL;
			numTransports = 0;
			return 0;
		}
	}
	atexit(CloseListeners);

	if (!SetAuthentication(numTransports, slistenObjs, &sauthDataEntries))
	{
	    fvwm_debug(__func__, "[%s][fsm_init] <<ERROR>> -- "
		       "Could not set authorization\n", module_name);
	    CloseListeners();
	    return 0;
	}

	if (FIceAddConnectionWatch(&ice_watch_fd, NULL) == 0)
	{
		fvwm_debug(__func__,
			   "[%s][fsm_init] <<ERROR>> -- "
			   "FIceAddConnectionWatch failed\n", module_name);
		FreeAuthenticationData(numTransports, &sauthDataEntries);
		CloseListeners();
		return 0;
	}

	ice_fd = fxmalloc(sizeof(int) * numTransports + 1);
	for (i = 0; i < numTransports; i++)
	{
		ice_fd[i] = FIceGetListenConnectionNumber(slistenObjs[i]);
	}

	networkIds = FIceComposeNetworkIdList(numTransports, slistenObjs);
	xasprintf(&p, "SESSION_MANAGER=%s", networkIds);
	putenv(p);

#ifdef FVWM_DEBUG_FSM
	fvwm_debug(__func__, "[%s][fsm_init] OK: %i\n", module_name,
		   numTransports);
	fvwm_debug(__func__, "\t%s\n", p);
#endif

	fsm_init_succeed = True;
	return 1;
}

void fsm_fdset(fd_set *in_fdset)
{
	int i;
	flist *l = fsm_ice_conn_list;
	fsm_ice_conn_t *fic;

	if (!SessionSupport || !fsm_init_succeed)
	{
		return;
	}

	for (i = 0; i < numTransports; i++)
	{
		FD_SET(ice_fd[i], in_fdset);
	}
	while(l != NULL)
	{
		fic = (fsm_ice_conn_t *)l->object;
		/* libICE can report other connections through its watch callback. */
		if (fic->fd >= 0 && fic->fd < FD_SETSIZE)
		{
			FD_SET(fic->fd, in_fdset);
		}
		l = l->next;
	}

}

Bool fsm_process(fd_set *in_fdset)
{
	int i;
	flist *l;
	fsm_ice_conn_t *fic;

	if (!SessionSupport || !fsm_init_succeed)
	{
		return False;
	}

	if (pending_ice_conn_list != NULL)
	{
		CompletNewConnectionMsg();
	}
	l = fsm_ice_conn_list;
	while(l != NULL)
	{
		fic = (fsm_ice_conn_t *)l->object;
		/* A close callback can remove the current watch/list node. */
		l = l->next;
		if (fic->fd >= 0 && fic->fd < FD_SETSIZE &&
		    FD_ISSET(fic->fd, in_fdset))
		{
			ProcessIceMsg(fic);
		}
	}
	for (i = 0; i < numTransports; i++)
	{
		if (FD_ISSET(ice_fd[i], in_fdset))
		{
			NewConnectionMsg(i);
		}
	}
	if (pending_ice_conn_list != NULL)
	{
		return True;
	}
	return False;
}

/* Keep session admission and watch ownership in their existing owner. */
size_t fsm_pollfds(struct pollfd *fds, size_t capacity)
{
	fd_set inputs;
	size_t count = 0;

	if (!SessionSupport || !fsm_init_succeed)
	{
		return 0;
	}
	FD_ZERO(&inputs);
	fsm_fdset(&inputs);
	for (int fd = 0; fd < FD_SETSIZE; fd++)
	{
		if (!FD_ISSET(fd, &inputs))
		{
			continue;
		}
		if (fds != NULL && count < capacity)
		{
			fds[count].fd = fd;
			fds[count].events = POLLIN;
			fds[count].revents = 0;
		}
		count++;
	}
	return count;
}

Bool fsm_process_pollfds(const struct pollfd *fds, size_t count)
{
	fd_set inputs;

	if (!SessionSupport || !fsm_init_succeed)
	{
		return False;
	}
	FD_ZERO(&inputs);
	for (size_t i = 0; i < count; i++)
	{
		if (fds[i].fd >= 0 && fds[i].fd < FD_SETSIZE &&
			(fds[i].revents & (POLLIN | POLLHUP | POLLERR)))
		{
			FD_SET(fds[i].fd, &inputs);
		}
	}
	return fsm_process(&inputs);
}

/* this try to explain to ksmserver and various sm poxies that they should not
 * connect our non XSMP award leader window to the top level sm */
void fsm_proxy(Display *dpy, Window win, char *sm)
{
	char *client_id;
	flist *l = running_list;
	Bool found = False;

	if (!SessionSupport || !fsm_init_succeed)
	{
		return;
	}

	client_id = GetClientID(dpy, win);

	if (client_id != NULL && strcmp("0", client_id) != 0)
	{
		for (l = running_list; l; l = l->next)
		{
			fsm_client_t *client = (fsm_client_t *) l->object;

			if (client->clientId &&
			    strcmp(client->clientId, client_id) == 0)
			{
				found = 1;
				break;
			}
		}
	}

	XFree(client_id);
	if (found)
	{
		return;
	}

	set_session_manager(dpy, win, sm);
}

void fsm_close(void)
{
	if (!SessionSupport || !fsm_init_succeed)
	{
		return;
	}
	FreeAuthenticationData(numTransports, &sauthDataEntries);
}
