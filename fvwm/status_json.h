#ifndef FVWM_STATUS_JSON_H
#define FVWM_STATUS_JSON_H

#include "libs/FScreen.h"

struct FvwmWindow;

typedef bool (*fvwm_status_desk_urgent_fn)(struct monitor *, int);
typedef int (*fvwm_status_desk_count_fn)(struct monitor *, int);

cJSON *
fvwm_status_json_create(const char *current_screen, const char *desktop_mode,
    const monitor_topology_snapshot *topology, struct monitors *legacy_monitors,
    const struct FvwmWindow   *focused_window,
    fvwm_status_desk_urgent_fn desk_is_urgent,
    fvwm_status_desk_count_fn  desk_client_count);

#endif
