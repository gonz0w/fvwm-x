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

#include "monitor_scale_config.h"

#include "libs/log.h"

#include <stdlib.h>
#include <string.h>

static monitor_scale_config monitor_scale_config_state;

static void *
scale_config_allocate(void *context, size_t size)
{
	(void)context;
	return malloc(size);
}

static void
scale_config_deallocate(void *context, void *pointer)
{
	(void)context;
	free(pointer);
}

static void
scale_config_diagnostic(void *context, const char *reason, const char *name)
{
	(void)context;
	(void)name;
	fvwm_debug(__func__, "%s", reason);
}

static void
diagnose(
    const monitor_scale_config *config, const char *reason, const char *name)
{
	if (config != NULL && config->diagnostic != NULL) {
		config->diagnostic(config->diagnostic_context, reason, name);
	}
}

static bool
allocator_is_valid(const monitor_scale_config_allocator *allocator)
{
	return allocator != NULL && allocator->allocate != NULL
	       && allocator->deallocate != NULL;
}

static monitor_scale_override *
find_override(const monitor_scale_config *config, const char *name,
    monitor_scale_override **previous)
{
	monitor_scale_override *current;
	monitor_scale_override *prior = NULL;

	if (previous != NULL) {
		*previous = NULL;
	}
	if (config == NULL || name == NULL) {
		return NULL;
	}
	for (current = config->overrides; current != NULL;
	    current  = current->next) {
		if (strcmp(current->name, name) == 0) {
			if (previous != NULL) {
				*previous = prior;
			}
			return current;
		}
		prior = current;
	}
	return NULL;
}

monitor_scale_config *
monitor_scale_config_get(void)
{
	return &monitor_scale_config_state;
}

void
monitor_scale_config_shutdown(void)
{
	monitor_scale_override *current = monitor_scale_config_state.overrides;

	while (current != NULL) {
		monitor_scale_override *next = current->next;

		if (monitor_scale_config_state.allocator.deallocate != NULL) {
			monitor_scale_config_state.allocator.deallocate(
			    monitor_scale_config_state.allocator.context,
			    current->name);
			monitor_scale_config_state.allocator.deallocate(
			    monitor_scale_config_state.allocator.context,
			    current);
		}
		current = next;
	}
	memset(
	    &monitor_scale_config_state, 0, sizeof(monitor_scale_config_state));
}

void
monitor_scale_config_initialize_with(
    const monitor_scale_config_allocator *allocator,
    monitor_scale_config_diagnostic_fn diagnostic, void *diagnostic_context)
{
	monitor_scale_config_shutdown();
	if (!allocator_is_valid(allocator)) {
		return;
	}
	monitor_scale_config_state.allocator	      = *allocator;
	monitor_scale_config_state.diagnostic	      = diagnostic;
	monitor_scale_config_state.diagnostic_context = diagnostic_context;
	monitor_scale_config_state.fallback.quarters =
	    MONITOR_SCALE_QUARTERS_MIN;
}

void
monitor_scale_config_initialize(void)
{
	monitor_scale_config_allocator allocator = {
	    .allocate	= scale_config_allocate,
	    .deallocate = scale_config_deallocate,
	    .context	= NULL,
	};

	monitor_scale_config_initialize_with(
	    &allocator, scale_config_diagnostic, NULL);
}

bool
monitor_scale_config_declare_override(
    monitor_scale_config *config, const char *name, const char *factor_text)
{
	monitor_scale_override *candidate;
	monitor_scale_override *current;
	monitor_scale_override *previous;
	monitor_scale_factor	factor;
	size_t			name_size;

	if (config == NULL || !allocator_is_valid(&config->allocator)) {
		return false;
	}
	if (config->finalized) {
		diagnose(
		    config, "scale configuration is already finalized", name);
		return false;
	}
	if (name == NULL || name[0] == '\0') {
		diagnose(config, "monitor name is missing", name);
		return false;
	}
	if (!monitor_scale_parse_factor(factor_text, &factor)) {
		diagnose(
		    config, "scale factor is not canonical or supported", name);
		return false;
	}

	candidate = config->allocator.allocate(
	    config->allocator.context, sizeof(*candidate));
	if (candidate == NULL) {
		diagnose(config, "could not allocate scale override", name);
		return false;
	}
	memset(candidate, 0, sizeof(*candidate));
	name_size = strlen(name) + 1;
	candidate->name =
	    config->allocator.allocate(config->allocator.context, name_size);
	if (candidate->name == NULL) {
		config->allocator.deallocate(
		    config->allocator.context, candidate);
		diagnose(config, "could not copy scale override name", name);
		return false;
	}
	memcpy(candidate->name, name, name_size);
	candidate->factor = factor;

	current = find_override(config, name, &previous);
	if (current == NULL) {
		candidate->next	  = config->overrides;
		config->overrides = candidate;
		return true;
	}
	candidate->next = current->next;
	if (previous == NULL) {
		config->overrides = candidate;
	} else {
		previous->next = candidate;
	}
	config->allocator.deallocate(config->allocator.context, current->name);
	config->allocator.deallocate(config->allocator.context, current);
	return true;
}

bool
monitor_scale_config_declare_fallback(
    monitor_scale_config *config, const char *factor_text)
{
	monitor_scale_factor candidate;

	if (config == NULL) {
		return false;
	}
	if (config->finalized) {
		diagnose(
		    config, "scale configuration is already finalized", NULL);
		return false;
	}
	if (!monitor_scale_parse_factor(factor_text, &candidate)) {
		diagnose(config,
		    "fallback factor is not canonical or supported", NULL);
		return false;
	}
	config->fallback     = candidate;
	config->has_fallback = true;
	return true;
}

monitor_scale_match_status
monitor_scale_config_match_override(const monitor_scale_config *config,
    const char *name, const monitor_topology_snapshot *snapshot,
    monitor_scale_factor *factor)
{
	monitor_scale_override *override;
	int			i;
	int			match_count = 0;

	if (factor != NULL) {
		factor->quarters = MONITOR_SCALE_QUARTERS_MIN;
	}
	if (config == NULL || name == NULL || factor == NULL) {
		return MONITOR_SCALE_MATCH_NONE;
	}
	override = find_override(config, name, NULL);
	if (override == NULL) {
		return MONITOR_SCALE_MATCH_NONE;
	}
	if (snapshot != NULL && snapshot->monitors != NULL
	    && snapshot->monitor_count > 0) {
		for (i = 0; i < snapshot->monitor_count; i++) {
			if (snapshot->monitors[i].name != NULL
			    && strcmp(snapshot->monitors[i].name, name) == 0) {
				match_count++;
			}
		}
	}
	if (match_count == 0) {
		diagnose(config, "retained scale override has no active match",
		    name);
		return MONITOR_SCALE_MATCH_NONE;
	}
	if (match_count > 1) {
		diagnose(config, "scale override name is ambiguous", name);
		return MONITOR_SCALE_MATCH_AMBIGUOUS;
	}
	*factor = override->factor;
	return MONITOR_SCALE_MATCH_UNIQUE;
}

bool
monitor_scale_config_is_finalized(const monitor_scale_config *config)
{
	return config != NULL && config->finalized;
}
