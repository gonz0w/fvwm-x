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

#include "fvwm/topology_event_adapter.h"

#include <X11/extensions/randr.h>
#include <assert.h>
#include <string.h>

typedef struct
{
	int  update_count;
	int  mark_count;
	int  refresh_count;
	int  downstream_count;
	char trace[3];
	int  trace_length;
} callback_state;

static fvwm_topology_event_input
make_input(int event_type, int randr_event_base, int randr_subtype,
    Window event_window, Window root_window)
{
	fvwm_topology_event_input input = {
	    .event_type	      = event_type,
	    .randr_event_base = randr_event_base,
	    .randr_subtype    = randr_subtype,
	    .event_window     = event_window,
	    .root_window      = root_window,
	};

	return input;
}

static void
record_update(void *context, XEvent *event)
{
	callback_state *state = context;

	assert(event != NULL);
	state->update_count++;
	state->trace[state->trace_length++] = 'u';
}

static void
record_mark(void *context)
{
	callback_state *state = context;

	state->mark_count++;
	state->trace[state->trace_length++] = 'm';
}

static fvwm_topology_event_class
dispatch_input(const fvwm_topology_event_input *input, callback_state *state)
{
	const fvwm_topology_event_callbacks callbacks = {
	    .update_configuration = record_update,
	    .mark_dirty		  = record_mark,
	};
	XEvent event = {0};

	return fvwm_topology_event_dispatch(input, &event, &callbacks, state);
}

static void
test_classifies_topology_events(void)
{
	const int    randr_base		 = 80;
	const Window root		 = 0x100;
	const int    accepted_subtypes[] = {
	       RRNotify_CrtcChange,
	       RRNotify_OutputChange,
	       RRNotify_OutputProperty,
	       RRNotify_ResourceChange,
	   };
	const int rejected_subtypes[] = {
	    RRNotify_ProviderChange,
	    RRNotify_ProviderProperty,
	    RRNotify_Lease,
	    99,
	};
	size_t			  i;
	fvwm_topology_event_input input;

	input = make_input(ConfigureNotify, randr_base, 0, root, root);
	assert(fvwm_topology_event_classify(&input)
	       == FVWM_TOPOLOGY_EVENT_ROOT_CONFIGURE);
	input.event_window = root + 1;
	assert(fvwm_topology_event_classify(&input)
	       == FVWM_TOPOLOGY_EVENT_REJECTED);

	input = make_input(
	    randr_base + RRScreenChangeNotify, randr_base, 0, root, root);
	assert(fvwm_topology_event_classify(&input)
	       == FVWM_TOPOLOGY_EVENT_SCREEN_CHANGE);

	for (i = 0; i < sizeof(accepted_subtypes) / sizeof(*accepted_subtypes);
	    i++) {
		input = make_input(randr_base + RRNotify, randr_base,
		    accepted_subtypes[i], root, root);
		assert(fvwm_topology_event_classify(&input)
		       == FVWM_TOPOLOGY_EVENT_RANDR_NOTIFY);
	}
	for (i = 0; i < sizeof(rejected_subtypes) / sizeof(*rejected_subtypes);
	    i++) {
		input = make_input(randr_base + RRNotify, randr_base,
		    rejected_subtypes[i], root, root);
		assert(fvwm_topology_event_classify(&input)
		       == FVWM_TOPOLOGY_EVENT_REJECTED);
	}

	input = make_input(PropertyNotify, randr_base, 0, root, root);
	assert(fvwm_topology_event_classify(&input)
	       == FVWM_TOPOLOGY_EVENT_REJECTED);
}

static void
test_dispatch_orders_update_before_mark(void)
{
	const int		  randr_base = 80;
	const Window		  root	     = 0x100;
	callback_state		  state	     = {0};
	fvwm_topology_event_input input	     = make_input(
		 randr_base + RRScreenChangeNotify, randr_base, 0, root, root);

	assert(dispatch_input(&input, &state)
	       == FVWM_TOPOLOGY_EVENT_SCREEN_CHANGE);
	assert(state.update_count == 1);
	assert(state.mark_count == 1);
	assert(state.trace_length == 2);
	assert(memcmp(state.trace, "um", 2) == 0);
	assert(state.refresh_count == 0);
	assert(state.downstream_count == 0);
}

static void
test_dispatch_marks_only_accepted_events(void)
{
	const int    randr_base	      = 80;
	const Window root	      = 0x100;
	const int    accepted_types[] = {
	       ConfigureNotify,
	       randr_base + RRNotify,
	       randr_base + RRNotify,
	       randr_base + RRNotify,
	       randr_base + RRNotify,
	   };
	const int accepted_subtypes[] = {
	    0,
	    RRNotify_CrtcChange,
	    RRNotify_OutputChange,
	    RRNotify_OutputProperty,
	    RRNotify_ResourceChange,
	};
	const int rejected_subtypes[] = {
	    RRNotify_ProviderChange,
	    RRNotify_ProviderProperty,
	    RRNotify_Lease,
	};
	size_t i;

	for (i = 0; i < sizeof(accepted_types) / sizeof(*accepted_types); i++) {
		callback_state		  state = {0};
		fvwm_topology_event_input input = make_input(accepted_types[i],
		    randr_base, accepted_subtypes[i], root, root);

		assert(dispatch_input(&input, &state)
		       != FVWM_TOPOLOGY_EVENT_REJECTED);
		assert(state.update_count == 0);
		assert(state.mark_count == 1);
		assert(state.trace_length == 1);
		assert(state.trace[0] == 'm');
		assert(state.refresh_count == 0);
		assert(state.downstream_count == 0);
	}

	for (i = 0; i < sizeof(rejected_subtypes) / sizeof(*rejected_subtypes);
	    i++) {
		callback_state		  state = {0};
		fvwm_topology_event_input input =
		    make_input(randr_base + RRNotify, randr_base,
			rejected_subtypes[i], root, root);

		assert(dispatch_input(&input, &state)
		       == FVWM_TOPOLOGY_EVENT_REJECTED);
		assert(state.update_count == 0);
		assert(state.mark_count == 0);
		assert(state.trace_length == 0);
		assert(state.refresh_count == 0);
		assert(state.downstream_count == 0);
	}

	{
		callback_state		  state = {0};
		fvwm_topology_event_input input =
		    make_input(ConfigureNotify, randr_base, 0, root + 1, root);

		assert(dispatch_input(&input, &state)
		       == FVWM_TOPOLOGY_EVENT_REJECTED);
		assert(state.update_count == 0);
		assert(state.mark_count == 0);
		assert(state.refresh_count == 0);
		assert(state.downstream_count == 0);
	}
}

static void
test_timeout_min_preserves_absent_zero_and_positive(void)
{
	assert(fvwm_topology_timeout_min(-1, -1) == -1);
	assert(fvwm_topology_timeout_min(-1, 23) == 23);
	assert(fvwm_topology_timeout_min(42, -1) == 42);
	assert(fvwm_topology_timeout_min(0, 42) == 0);
	assert(fvwm_topology_timeout_min(42, 0) == 0);
	assert(fvwm_topology_timeout_min(17, 42) == 17);
	assert(fvwm_topology_timeout_min(42, 17) == 17);
}

static void
test_only_changed_requests_downstream_work(void)
{
	assert(!fvwm_topology_refresh_requests_downstream(
	    FSCREEN_TOPOLOGY_REFRESH_FAILED));
	assert(!fvwm_topology_refresh_requests_downstream(
	    FSCREEN_TOPOLOGY_REFRESH_UNCHANGED));
	assert(fvwm_topology_refresh_requests_downstream(
	    FSCREEN_TOPOLOGY_REFRESH_CHANGED));
}

int
main(void)
{
	test_classifies_topology_events();
	test_dispatch_orders_update_before_mark();
	test_dispatch_marks_only_accepted_events();
	test_timeout_min_preserves_absent_zero_and_positive();
	test_only_changed_requests_downstream_work();
	return 0;
}
