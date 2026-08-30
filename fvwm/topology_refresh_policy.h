/* -*-c-*- */

#ifndef FVWM_TOPOLOGY_REFRESH_POLICY_H
#define FVWM_TOPOLOGY_REFRESH_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#define FVWM_TOPOLOGY_QUIET_MS 50
#define FVWM_TOPOLOGY_MAX_LATENCY_MS 250

typedef struct
{
	bool	 dirty;
	uint64_t first_dirty_ns;
	uint64_t last_dirty_ns;
} fvwm_topology_refresh_state;

#define FVWM_TOPOLOGY_REFRESH_STATE_INIT {false, 0, 0}

void
fvwm_topology_refresh_init(fvwm_topology_refresh_state *state);
void
fvwm_topology_refresh_mark(fvwm_topology_refresh_state *state, uint64_t now_ns);
bool
fvwm_topology_refresh_is_due(
    const fvwm_topology_refresh_state *state, uint64_t now_ns);
int
fvwm_topology_refresh_timeout_ms(
    const fvwm_topology_refresh_state *state, uint64_t now_ns);
void
fvwm_topology_refresh_complete(fvwm_topology_refresh_state *state);

#endif
