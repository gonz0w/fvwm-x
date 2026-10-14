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

#include "cJSON.h"
#include "monitor_topology.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *
default_malloc(void *context, size_t size)
{
	return malloc(size);
}

static void *
default_calloc(void *context, size_t count, size_t size)
{
	return calloc(count, size);
}

static void
default_free(void *context, void *pointer)
{
	free(pointer);
}

static bool
allocator_resolve(const monitor_topology_allocator *requested,
    monitor_topology_allocator			   *resolved)
{
	if (resolved == NULL) {
		return false;
	}
	if (requested == NULL) {
		*resolved = (monitor_topology_allocator){
		    .context   = NULL,
		    .malloc_fn = default_malloc,
		    .calloc_fn = default_calloc,
		    .free_fn   = default_free,
		};
		return true;
	}
	if (requested->malloc_fn == NULL || requested->calloc_fn == NULL
	    || requested->free_fn == NULL) {
		return false;
	}
	*resolved = *requested;
	return true;
}

static bool
allocation_count_is_safe(size_t count, size_t element_size)
{
	return element_size != 0 && count <= SIZE_MAX / element_size;
}

static bool
geometry_is_representable(const monitor_topology_monitor *monitor)
{
	return monitor->width > 0 && monitor->height > 0
	       && monitor->x <= INT_MAX - monitor->width
	       && monitor->y <= INT_MAX - monitor->height;
}

static bool
is_utf8_continuation(unsigned char byte)
{
	return byte >= 0x80 && byte <= 0xbf;
}

static bool
is_valid_utf8(const char *string)
{
	const unsigned char *bytes = (const unsigned char *)string;

	if (bytes == NULL || *bytes == '\0') {
		return false;
	}
	while (*bytes != '\0') {
		if (*bytes <= 0x7f) {
			bytes++;
		} else if (*bytes >= 0xc2 && *bytes <= 0xdf
			   && is_utf8_continuation(bytes[1])) {
			bytes += 2;
		} else if (*bytes == 0xe0 && bytes[1] >= 0xa0
			   && bytes[1] <= 0xbf
			   && is_utf8_continuation(bytes[2])) {
			bytes += 3;
		} else if (*bytes >= 0xe1 && *bytes <= 0xec
			   && is_utf8_continuation(bytes[1])
			   && is_utf8_continuation(bytes[2])) {
			bytes += 3;
		} else if (*bytes == 0xed && bytes[1] >= 0x80
			   && bytes[1] <= 0x9f
			   && is_utf8_continuation(bytes[2])) {
			bytes += 3;
		} else if (*bytes >= 0xee && *bytes <= 0xef
			   && is_utf8_continuation(bytes[1])
			   && is_utf8_continuation(bytes[2])) {
			bytes += 3;
		} else if (*bytes == 0xf0 && bytes[1] >= 0x90
			   && bytes[1] <= 0xbf && is_utf8_continuation(bytes[2])
			   && is_utf8_continuation(bytes[3])) {
			bytes += 4;
		} else if (*bytes >= 0xf1 && *bytes <= 0xf3
			   && is_utf8_continuation(bytes[1])
			   && is_utf8_continuation(bytes[2])
			   && is_utf8_continuation(bytes[3])) {
			bytes += 4;
		} else if (*bytes == 0xf4 && bytes[1] >= 0x80
			   && bytes[1] <= 0x8f && is_utf8_continuation(bytes[2])
			   && is_utf8_continuation(bytes[3])) {
			bytes += 4;
		} else {
			return false;
		}
	}
	return true;
}

static int
compare_outputs(const void *left_pointer, const void *right_pointer)
{
	const RROutput left  = *(const RROutput *)left_pointer;
	const RROutput right = *(const RROutput *)right_pointer;

	return left < right ? -1 : left > right ? 1 : 0;
}

static int
compare_monitors(const void *left_pointer, const void *right_pointer)
{
	const monitor_topology_monitor *left  = left_pointer;
	const monitor_topology_monitor *right = right_pointer;

	if (left->name_atom != right->name_atom) {
		return left->name_atom < right->name_atom ? -1 : 1;
	}
	return strcmp(left->name, right->name);
}

static int
deduplicate_outputs(RROutput *outputs, int output_count)
{
	int read_index;
	int write_index;

	if (output_count < 2) {
		return output_count;
	}
	write_index = 1;
	for (read_index = 1; read_index < output_count; read_index++) {
		if (outputs[read_index] != outputs[write_index - 1]) {
			outputs[write_index++] = outputs[read_index];
		}
	}
	return write_index;
}

static bool
monitor_is_valid(const monitor_topology_monitor *monitor)
{
	int output_index;

	if (monitor == NULL || monitor->name_atom == None
	    || !is_valid_utf8(monitor->name)
	    || !geometry_is_representable(monitor) || monitor->mm_width < 0
	    || monitor->mm_height < 0
	    || (monitor->primary != False && monitor->primary != True)
	    || monitor->output_count < 0
	    || (monitor->output_count > 0 && monitor->outputs == NULL)
	    || !allocation_count_is_safe(
		(size_t)monitor->output_count, sizeof(*monitor->outputs))) {
		return false;
	}
	for (output_index = 0; output_index < monitor->output_count;
	    output_index++) {
		if (monitor->outputs[output_index] == None) {
			return false;
		}
		if (output_index > 0
		    && monitor->outputs[output_index - 1]
			   >= monitor->outputs[output_index]) {
			return false;
		}
	}
	return true;
}

monitor_topology_snapshot *
monitor_topology_snapshot_create(const monitor_topology_monitor *monitors,
    int monitor_count, const monitor_topology_allocator *allocator)
{
	monitor_topology_allocator resolved;
	monitor_topology_snapshot *snapshot;
	int			   monitor_index;

	if (monitors == NULL || monitor_count <= 0
	    || !allocation_count_is_safe(
		(size_t)monitor_count, sizeof(*snapshot->monitors))
	    || !allocator_resolve(allocator, &resolved)) {
		return NULL;
	}
	snapshot = resolved.calloc_fn(resolved.context, 1, sizeof(*snapshot));
	if (snapshot == NULL) {
		return NULL;
	}
	snapshot->allocator	= resolved;
	snapshot->monitor_count = monitor_count;
	snapshot->monitors	= resolved.calloc_fn(resolved.context,
		 (size_t)monitor_count, sizeof(*snapshot->monitors));
	if (snapshot->monitors == NULL) {
		goto fail;
	}

	for (monitor_index = 0; monitor_index < monitor_count;
	    monitor_index++) {
		const monitor_topology_monitor *source =
		    &monitors[monitor_index];
		monitor_topology_monitor *copy =
		    &snapshot->monitors[monitor_index];
		size_t name_size;

		if (source->name_atom == None || !is_valid_utf8(source->name)
		    || !geometry_is_representable(source)
		    || source->mm_width < 0 || source->mm_height < 0
		    || (source->primary != False && source->primary != True)
		    || source->output_count < 0
		    || (source->output_count > 0 && source->outputs == NULL)
		    || !allocation_count_is_safe((size_t)source->output_count,
			sizeof(*source->outputs))) {
			goto fail;
		}
		*copy	      = *source;
		copy->name    = NULL;
		copy->outputs = NULL;
		name_size     = strlen(source->name) + 1;
		if (name_size == 0) {
			goto fail;
		}
		copy->name = resolved.malloc_fn(resolved.context, name_size);
		if (copy->name == NULL) {
			goto fail;
		}
		memcpy(copy->name, source->name, name_size);
		if (source->output_count > 0) {
			copy->outputs = resolved.calloc_fn(resolved.context,
			    (size_t)source->output_count,
			    sizeof(*copy->outputs));
			if (copy->outputs == NULL) {
				goto fail;
			}
			memcpy(copy->outputs, source->outputs,
			    (size_t)source->output_count
				* sizeof(*copy->outputs));
			qsort(copy->outputs, (size_t)source->output_count,
			    sizeof(*copy->outputs), compare_outputs);
			copy->output_count = deduplicate_outputs(
			    copy->outputs, source->output_count);
		}
	}
	qsort(snapshot->monitors, (size_t)snapshot->monitor_count,
	    sizeof(*snapshot->monitors), compare_monitors);
	if (!monitor_topology_snapshot_validate(snapshot)) {
		goto fail;
	}
	return snapshot;

fail:
	monitor_topology_snapshot_destroy(snapshot);
	return NULL;
}

bool
monitor_topology_snapshot_validate(const monitor_topology_snapshot *snapshot)
{
	int monitor_index;

	if (snapshot == NULL || snapshot->monitors == NULL
	    || snapshot->monitor_count <= 0
	    || !allocation_count_is_safe(
		(size_t)snapshot->monitor_count, sizeof(*snapshot->monitors))) {
		return false;
	}
	if (snapshot->root_fallback &&
	    (snapshot->monitor_count != 1 || snapshot->monitors[0].x != 0 ||
	     snapshot->monitors[0].y != 0 ||
	     snapshot->monitors[0].output_count != 0 ||
	     !snapshot->monitors[0].primary)) {
		return false;
	}
	for (monitor_index = 0; monitor_index < snapshot->monitor_count;
	    monitor_index++) {
		const monitor_topology_monitor *monitor =
		    &snapshot->monitors[monitor_index];

		if (!monitor_is_valid(monitor)) {
			return false;
		}
		if (monitor_index > 0) {
			const monitor_topology_monitor *previous =
			    &snapshot->monitors[monitor_index - 1];

			if (compare_monitors(previous, monitor) >= 0
			    || previous->name_atom == monitor->name_atom
			    || strcmp(previous->name, monitor->name) == 0) {
				return false;
			}
		}
	}
	return true;
}

bool
monitor_topology_snapshot_equal(const monitor_topology_snapshot *left,
    const monitor_topology_snapshot				*right)
{
	int monitor_index;

	if (!monitor_topology_snapshot_validate(left)
	    || !monitor_topology_snapshot_validate(right)
	    || left->monitor_count != right->monitor_count
	    || left->root_fallback != right->root_fallback) {
		return false;
	}
	for (monitor_index = 0; monitor_index < left->monitor_count;
	    monitor_index++) {
		const monitor_topology_monitor *left_monitor =
		    &left->monitors[monitor_index];
		const monitor_topology_monitor *right_monitor =
		    &right->monitors[monitor_index];

		if (left_monitor->name_atom != right_monitor->name_atom
		    || strcmp(left_monitor->name, right_monitor->name) != 0
		    || left_monitor->x != right_monitor->x
		    || left_monitor->y != right_monitor->y
		    || left_monitor->width != right_monitor->width
		    || left_monitor->height != right_monitor->height
		    || left_monitor->mm_width != right_monitor->mm_width
		    || left_monitor->mm_height != right_monitor->mm_height
		    || left_monitor->primary != right_monitor->primary
		    || left_monitor->output_count != right_monitor->output_count
		    || (left_monitor->output_count > 0
			&& memcmp(left_monitor->outputs, right_monitor->outputs,
			       (size_t)left_monitor->output_count
				   * sizeof(*left_monitor->outputs))
			       != 0)) {
			return false;
		}
	}
	return true;
}

monitor_topology_publish_result
monitor_topology_snapshot_publish(
    monitor_topology_snapshot **committed, monitor_topology_snapshot *candidate)
{
	monitor_topology_snapshot *old;

	if (committed == NULL
	    || !monitor_topology_snapshot_validate(candidate)) {
		monitor_topology_snapshot_destroy(candidate);
		return MONITOR_TOPOLOGY_PUBLISH_INVALID;
	}
	old = *committed;
	if (old == NULL) {
		candidate->epoch = 1;
		*committed	 = candidate;
		return MONITOR_TOPOLOGY_PUBLISH_INITIAL;
	}
	if (candidate == old) {
		return MONITOR_TOPOLOGY_PUBLISH_UNCHANGED;
	}
	if (!monitor_topology_snapshot_validate(old)) {
		monitor_topology_snapshot_destroy(candidate);
		return MONITOR_TOPOLOGY_PUBLISH_INVALID;
	}
	if (monitor_topology_snapshot_equal(old, candidate)) {
		monitor_topology_snapshot_destroy(candidate);
		return MONITOR_TOPOLOGY_PUBLISH_UNCHANGED;
	}
	if (old->epoch == UINT64_MAX) {
		monitor_topology_snapshot_destroy(candidate);
		return MONITOR_TOPOLOGY_PUBLISH_INVALID;
	}
	candidate->epoch = old->epoch + 1;
	*committed	 = candidate;
	monitor_topology_snapshot_destroy(old);
	return MONITOR_TOPOLOGY_PUBLISH_CHANGED;
}

void
monitor_topology_snapshot_destroy(monitor_topology_snapshot *snapshot)
{
	monitor_topology_allocator allocator;
	int			   monitor_index;

	if (snapshot == NULL) {
		return;
	}
	allocator = snapshot->allocator;
	if (allocator.free_fn == NULL) {
		allocator.context = NULL;
		allocator.free_fn = default_free;
	}
	if (snapshot->monitors != NULL) {
		for (monitor_index = 0; monitor_index < snapshot->monitor_count;
		    monitor_index++) {
			allocator.free_fn(allocator.context,
			    snapshot->monitors[monitor_index].name);
			allocator.free_fn(allocator.context,
			    snapshot->monitors[monitor_index].outputs);
		}
	}
	allocator.free_fn(allocator.context, snapshot->monitors);
	allocator.free_fn(allocator.context, snapshot);
}

static bool
json_add_number(cJSON *object, const char *key, double value)
{
	return cJSON_AddNumberToObject(object, key, value) != NULL;
}

static bool
json_add_uint64(cJSON *object, const char *key, uint64_t value)
{
	char encoded[sizeof("18446744073709551615")];
	int  length;

	length = snprintf(encoded, sizeof(encoded), "%" PRIu64, value);
	return length > 0 && (size_t)length < sizeof(encoded)
	       && cJSON_AddRawToObject(object, key, encoded) != NULL;
}

static bool
json_add_monitor(cJSON *monitors, const monitor_topology_monitor *monitor)
{
	cJSON *item	= NULL;
	cJSON *geometry = NULL;
	cJSON *physical = NULL;
	cJSON *outputs	= NULL;
	int    output_index;

	item = cJSON_CreateObject();
	if (item == NULL
	    || cJSON_AddStringToObject(item, "name", monitor->name) == NULL
	    || !json_add_number(item, "name_atom", (double)monitor->name_atom)
	    || cJSON_AddBoolToObject(item, "primary", monitor->primary)
		   == NULL) {
		goto fail;
	}

	geometry = cJSON_CreateObject();
	if (geometry == NULL || !json_add_number(geometry, "x", monitor->x)
	    || !json_add_number(geometry, "y", monitor->y)
	    || !json_add_number(geometry, "width", monitor->width)
	    || !json_add_number(geometry, "height", monitor->height)
	    || !cJSON_AddItemToObject(item, "geometry", geometry)) {
		goto fail;
	}
	geometry = NULL;

	physical = cJSON_CreateObject();
	if (physical == NULL
	    || !json_add_number(physical, "width", monitor->mm_width)
	    || !json_add_number(physical, "height", monitor->mm_height)
	    || !cJSON_AddItemToObject(item, "physical_mm", physical)) {
		goto fail;
	}
	physical = NULL;

	outputs = cJSON_CreateArray();
	if (outputs == NULL) {
		goto fail;
	}
	for (output_index = 0; output_index < monitor->output_count;
	    output_index++) {
		cJSON *output =
		    cJSON_CreateNumber((double)monitor->outputs[output_index]);

		if (output == NULL || !cJSON_AddItemToArray(outputs, output)) {
			cJSON_Delete(output);
			goto fail;
		}
	}
	if (!cJSON_AddItemToObject(item, "outputs", outputs)) {
		goto fail;
	}
	outputs = NULL;

	if (!cJSON_AddItemToArray(monitors, item)) {
		goto fail;
	}
	return true;

fail:
	cJSON_Delete(outputs);
	cJSON_Delete(physical);
	cJSON_Delete(geometry);
	cJSON_Delete(item);
	return false;
}

bool
monitor_topology_json_add(
    cJSON *parent, const char *key, const monitor_topology_snapshot *snapshot)
{
	cJSON *topology = NULL;
	cJSON *monitors = NULL;
	int    monitor_index;

	if (parent == NULL || key == NULL || snapshot == NULL
	    || snapshot->epoch == 0
	    || !monitor_topology_snapshot_validate(snapshot)) {
		return false;
	}
	topology = cJSON_CreateObject();
	if (topology == NULL
	    || !json_add_uint64(topology, "epoch", snapshot->epoch)) {
		goto fail;
	}
	monitors = cJSON_CreateArray();
	if (monitors == NULL) {
		goto fail;
	}
	for (monitor_index = 0; monitor_index < snapshot->monitor_count;
	    monitor_index++) {
		if (!json_add_monitor(
			monitors, &snapshot->monitors[monitor_index])) {
			goto fail;
		}
	}
	if (!cJSON_AddItemToObject(topology, "monitors", monitors)) {
		goto fail;
	}
	monitors = NULL;
	if (!cJSON_AddItemToObject(parent, key, topology)) {
		goto fail;
	}
	return true;

fail:
	cJSON_Delete(monitors);
	cJSON_Delete(topology);
	return false;
}
