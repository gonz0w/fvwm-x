#ifndef FVWM_MONITOR_SCALE_CONFIG_H
#define FVWM_MONITOR_SCALE_CONFIG_H

#include "libs/monitor_scale.h"

#include <stdbool.h>
#include <stddef.h>

struct monitor;
struct monitors;

typedef struct monitor_scale_override
{
	char			      *name;
	monitor_scale_factor	       factor;
	struct monitor_scale_override *next;
} monitor_scale_override;

typedef struct
{
	void *(*allocate)(void *context, size_t size);
	void (*deallocate)(void *context, void *pointer);
	void *context;
} monitor_scale_config_allocator;

typedef void (*monitor_scale_config_diagnostic_fn)(
    void *context, const char *reason, const char *name);

typedef struct
{
	monitor_scale_override		  *overrides;
	monitor_scale_factor		   fallback;
	bool				   has_fallback;
	bool				   finalized;
	monitor_scale_config_allocator	   allocator;
	monitor_scale_config_diagnostic_fn diagnostic;
	void				  *diagnostic_context;
} monitor_scale_config;

monitor_scale_config *
monitor_scale_config_get(void);
void
monitor_scale_config_initialize(void);
void
monitor_scale_config_initialize_with(
    const monitor_scale_config_allocator *allocator,
    monitor_scale_config_diagnostic_fn diagnostic, void *diagnostic_context);
void
monitor_scale_config_shutdown(void);

bool
monitor_scale_config_declare_override(
    monitor_scale_config *config, const char *name, const char *factor_text);
bool
monitor_scale_config_declare_fallback(
    monitor_scale_config *config, const char *factor_text);
monitor_scale_match_status
monitor_scale_config_match_override(const monitor_scale_config *config,
    const char *name, const monitor_topology_snapshot *snapshot,
    monitor_scale_factor *factor);
bool
monitor_scale_config_is_finalized(const monitor_scale_config *config);
void
monitor_scale_config_associate_monitor(const monitor_scale_config *config,
    const monitor_topology_monitor				  *fact,
    const monitor_topology_snapshot *snapshot, struct monitor *monitor);
bool
monitor_scale_config_recompute(void);
bool
monitor_scale_config_results_valid(void);

#ifdef MONITOR_SCALE_CONFIG_TESTING
bool
monitor_scale_config_recompute_with(const monitor_scale_config *config,
    const monitor_topology_snapshot			       *snapshot,
    struct monitors *runtime_monitors);
bool
monitor_scale_config_results_valid_with(
    const monitor_topology_snapshot *snapshot,
    struct monitors		    *runtime_monitors);
#endif

#endif
