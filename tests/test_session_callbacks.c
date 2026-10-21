/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/fsm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SmsNewClientProc new_client;
static SmsCallbacks callbacks;
static int mode, token;
static unsigned generated, replies, saves, properties;
static unsigned id_frees, previous_frees, cleanups, closes;
static char *generated_id, *previous_id;
static Bool refuse;

void __real_free(void *);
void __wrap_free(void *pointer)
{
	if (pointer != NULL && pointer == generated_id)
	{
		id_frees++;
		generated_id = NULL;
	}
	if (pointer != NULL && pointer == previous_id)
	{
		previous_frees++;
		previous_id = NULL;
	}
	__real_free(pointer);
}

Status __wrap_SmsInitialize(const char *vendor, const char *release,
	SmsNewClientProc callback, SmPointer data, IceHostBasedAuthProc auth,
	int length, char *error)
{
	(void)vendor; (void)release; (void)data; (void)auth; (void)length;
	new_client = callback;
	error[0] = '\0';
	/* Capture the actual private callbacks without opening listeners. */
	return 0;
}

IceConn __wrap_SmsGetIceConnection(SmsConn connection)
{
	assert(connection == (SmsConn)&token);
	return (IceConn)&token;
}

char *__wrap_SmsGenerateClientID(SmsConn connection)
{
	assert(connection == (SmsConn)&token);
	generated++;
	if (mode == 1 && refuse)
		return NULL;
	assert(generated_id == NULL);
	generated_id = strdup("fresh-session-client");
	assert(generated_id != NULL);
	return generated_id;
}

Status __wrap_SmsRegisterClientReply(SmsConn connection, char *id)
{
	assert(connection == (SmsConn)&token && id != NULL);
	assert(strcmp(id, "fresh-session-client") == 0);
	replies++;
	return !(mode == 2 && refuse);
}

void __wrap_SmsSaveYourself(SmsConn connection, int type, Bool shutdown,
	int interaction, Bool fast)
{
	assert(connection == (SmsConn)&token && replies > 0);
	assert(type == SmSaveLocal && !shutdown &&
		interaction == SmInteractStyleNone && !fast);
	assert(generated_id != NULL);
	saves++;
}

void __wrap_SmsReturnProperties(SmsConn connection, int count, SmProp **props)
{
	assert(connection == (SmsConn)&token && count == 0 && props == NULL);
	properties++;
}

void __wrap_SmsCleanUp(SmsConn connection)
{
	assert(connection == (SmsConn)&token);
	cleanups++;
}

void __wrap_IceSetShutdownNegotiation(IceConn connection, Bool negotiate)
{
	assert(connection == (IceConn)&token && !negotiate);
}

IceCloseStatus __wrap_IceCloseConnection(IceConn connection)
{
	assert(connection == (IceConn)&token);
	closes++;
	return IceClosedNow;
}

int main(int argc, char **argv)
{
	unsigned long mask = 0;
	char *reason = NULL;
	SmsConn connection = (SmsConn)&token;
	SmPointer client;

	assert(argc == 2);
	mode = atoi(argv[1]);
	assert(mode >= 0 && mode <= 4);
	assert(!fsm_init("session-callback-test") && new_client != NULL);
	assert(new_client(connection, NULL, &mask, &callbacks, &reason));
	assert(mask & SmsRegisterClientProcMask);
	assert(mask & SmsGetPropertiesProcMask);
	assert(reason == NULL);
	client = callbacks.register_client.manager_data;
	refuse = True;
	if (mode == 0)
	{
		previous_id = strdup("unrestored-client");
		assert(previous_id != NULL);
		assert(!callbacks.register_client.callback(
			connection, client, previous_id));
		assert(previous_frees == 1 && generated == 0 && replies == 0);
	}
	else if (mode == 1 || mode == 2)
	{
		assert(!callbacks.register_client.callback(
			connection, client, NULL));
		assert(generated == 1 && replies == (mode == 2));
		assert(id_frees == (mode == 2) && saves == 0);
	}
	refuse = False;
	assert(callbacks.register_client.callback(connection, client, NULL));
	if (mode == 4)
	{
		/* This proxy intentionally discards set/delete data. Each get,
		 * including one after a mutation, still completes exactly once. */
		callbacks.get_properties.callback(connection, client);
		SmProp **props = malloc(sizeof(*props));
		assert(props != NULL);
		props[0] = calloc(1, sizeof(**props));
		assert(props[0] != NULL);
		props[0]->name = strdup("test-property");
		props[0]->type = strdup(SmARRAY8);
		assert(props[0]->name && props[0]->type);
		callbacks.set_properties.callback(connection, client, 1, props);
		callbacks.get_properties.callback(connection, client);
		char **names = malloc(sizeof(*names));
		assert(names != NULL);
		names[0] = strdup("test-property");
		assert(names[0] != NULL);
		callbacks.delete_properties.callback(connection, client, 1, names);
		callbacks.get_properties.callback(connection, client);
		assert(properties == 3);
	}
	assert(saves == 0 && generated_id != NULL);
	callbacks.close_connection.callback(connection, client, 0, NULL);
	assert(cleanups == 1 && closes == 1);
	assert(generated_id == NULL && id_frees == 1 + (mode == 2));
	printf("session-callbacks case=%d: passed\n", mode);
	return 0;
}
