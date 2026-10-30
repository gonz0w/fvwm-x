#ifndef FVWM_SESSION_CLIENTS_H
#define FVWM_SESSION_CLIENTS_H

#include "libs/cJSON.h"
#include "session_clients_policy.h"

struct fvwm_query_spec;

cJSON *session_clients_query(const struct fvwm_query_spec *spec,
	const char **error);
/* Copies one fixed engine command; no FvwmWindow pointer escapes. */
cJSON *session_clients_action(const struct session_clients_request *request,
	char command[96], const char **error);
/* Final empty check, immediately before the shared Quit engine. */
bool session_clients_quit_ready(const char *generation);

#endif
