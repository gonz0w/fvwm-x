# Phase 1: Topology Snapshot and Live-X Harness - Pattern Map

**Mapped:** 2026-08-29
**Files analyzed:** 25 new/modified files
**Analogs found:** 21 / 25

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `libs/monitor_topology.c` | model | transform | `libs/ImageSafety.c` | role-match |
| `libs/monitor_topology.h` | model interface | transform | `libs/ImageSafety.h` | role-match |
| `libs/FScreen.c` | service / XRandR adapter | request-response + event-driven | existing `libs/FScreen.c` query/init paths | exact (in-place) |
| `libs/FScreen.h` | provider interface | request-response | existing `libs/FScreen.h` control/query API | exact (in-place) |
| `libs/meson.build` | config | batch | existing `libfvwm3_sources` list | exact (in-place) |
| `fvwm/topology_refresh_policy.c` | utility / policy | event-driven | `fvwm/event_loop_policy.c` | exact role/flow |
| `fvwm/topology_refresh_policy.h` | utility interface | event-driven | `fvwm/event_loop_policy.h` | exact role/flow |
| `fvwm/topology_event_adapter.c` | utility / event dispatch seam | event-driven | `fvwm/event_loop_policy.c` | role/flow match |
| `fvwm/topology_event_adapter.h` | utility interface / callback contract | event-driven | `fvwm/event_loop_policy.h` | role/flow match |
| `fvwm/events.c` | controller / event-loop coordinator | event-driven | existing RandR dispatch and timeout loop | exact (in-place) |
| `fvwm/eventmask.h` | config | event-driven | existing `XEVMASK_ROOTW` | exact (in-place) |
| `fvwm/builtins.c` | controller / serializer | request-response + file-I/O | existing `Status` and `PrintInfo` paths | exact (in-place) |
| `fvwm/meson.build` | config | batch | existing `fvwm_sources` list | exact (in-place) |
| `doc/fvwm3_manpage_source.adoc` | config / documentation | transform | existing `PrintInfo` and `Status` sections | exact (in-place) |
| `tests/test_monitor_topology.c` | test | transform | `tests/test_image_safety.c` | role-match |
| `tests/test_fscreen_topology.c` | focused adapter test | request-response | `tests/test_module_runtime.c` | role/fixture match |
| `tests/test_topology_refresh_policy.c` | test | event-driven | `tests/test_event_loop_policy.c` | exact role/flow |
| `tests/test_topology_event_adapter.c` | focused event/callback test | event-driven | `tests/test_event_loop_policy.c` | exact role/flow |
| `tests/run_in_linux.py` | platform verification router | batch + process execution | none in repository | none |
| `tests/live_x/live_x_harness.py` | test harness | event-driven + file-I/O | none in repository | none |
| `tests/live_x/xorg-dummy.conf` | config / fixture | request-response | none in repository | none |
| `tests/live_x/xrr_query_fault.c` | middleware / test shim | request-response | none in repository | none |
| `tests/meson.build` | config / test registration | batch | existing native test registrations | exact (in-place) |
| `meson.options` | config | batch | existing boolean documentation options | exact (in-place) |
| `.github/workflows/ccpp.yml` | config / CI | batch | existing `build` and `sanitizers` jobs | role-match (in-place) |

The list includes the two implied source-registration edits, `libs/meson.build`
and `fvwm/meson.build`; without them the new C units are not linked. No root
`meson.build` edit is needed: it already creates `py` and enters `tests/` after
building `fvwm3` (`meson.build:241-252,501-533`).

## Pattern Assignments

### `libs/monitor_topology.c` (model, transform)

**Analog:** `libs/ImageSafety.c`, supplemented by ownership/error patterns in
`fvwm/module_runtime.c`.

**Checked-validation pattern** (`libs/ImageSafety.c:3-16`):

```c
bool FImageSizeIsSafe(
	uint64_t width, uint64_t height, uint64_t max_dimension,
	size_t max_pixels, size_t *pixel_count)
{
	if (pixel_count == NULL || width == 0 || height == 0 ||
	    width > max_dimension || height > max_dimension ||
	    height > max_pixels || width > max_pixels / height)
	{
		return false;
	}

	*pixel_count = (size_t)(width * height);
	return true;
}
```

Copy the explicit predicate/result style, safe division-before-multiplication,
and `bool` result. For topology counts and output arrays, reject invalid counts
before `count * sizeof(*outputs)` and initialize all outputs before work.

**Idempotent destruction pattern** (`fvwm/module_runtime.c:82-101`):

```c
void module_destroy(fmodule *module)
{
	if (module == NULL)
	{
		return;
	}
	free(MOD_NAME(module));
	free(MOD_ALIAS(module));
	fqueue_remove_or_operate_all(
		&MOD_PIPEQUEUE(module), NULL, NULL, free, NULL);
	free(module);
}
```

Apply this ownership shape recursively: destroy every monitor-owned name and
output array, then the monitor array, then the snapshot; accepting `NULL` keeps
candidate-failure cleanup simple.

**Do not copy:** `ImageSafety.c` lacks the generated-header convention. New
`monitor_topology.c` must follow modern core files and include `config.h` first,
as in `fvwm/event_loop_policy.c:1-5`.

**Core model contract:** keep this unit free of X calls, `Scr`, `monitor_q`, and
logging. It owns detached candidate allocation/deep copy, UTF-8/name and geometry
validation, monitor/output canonical sorting, semantic equality, epoch
assignment, and destruction. Use recoverable `malloc`/`calloc`, not `fxcalloc`,
for candidate paths whose failure must preserve the committed object.

---

### `libs/monitor_topology.h` (model interface, transform)

**Analog:** `libs/ImageSafety.h`.

**Header/API pattern** (`libs/ImageSafety.h:1-15`):

```c
#ifndef FVWM_IMAGE_SAFETY_H
#define FVWM_IMAGE_SAFETY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool FImageSizeIsSafe(
	uint64_t width, uint64_t height, uint64_t max_dimension,
	size_t max_pixels, size_t *pixel_count);
#endif
```

Use an include guard derived from `MONITOR_TOPOLOGY`, standard integer/count
headers, domain structs with owned `char *name` and `RROutput *outputs`, and
`const monitor_topology_snapshot *` for read-only committed access. Keep XRandR
query types out of the pure API where possible; Atom/XID facts may use their
protocol-sized public types if required by callers.

Expose lifecycle/validation/equality/publication helpers narrowly. Do not expose
mutable global snapshot storage from this header.

---

### `libs/FScreen.c` (service / XRandR adapter)

**Analog:** its existing XRandR boundary, but replace—not copy—the live-mutation
algorithm.

**Imports and adapter placement** (`libs/FScreen.c:16-37`):

```c
#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <X11/extensions/randr.h>
#include <X11/extensions/Xrandr.h>

#include "FScreen.h"
#include "log.h"
```

Add `monitor_topology.h` beside the local library headers. Keep the connected
`Display *` and committed snapshot storage file-private, matching the current
`static Display *disp` state at `libs/FScreen.c:51-54`.

**Current query boundary to refactor** (`libs/FScreen.c:576-595`):

```c
static void
scan_screens(Display *dpy)
{
	XRRMonitorInfo *rrm;
	int i, n = 0;
	Window root = RootWindow(dpy, DefaultScreen(dpy));

	rrm = XRRGetMonitors(dpy, root, true, &n);
	if (n <= 0 && (!randr_initialised && monitor_get_count() == 0)) {
		fvwm_debug(__func__, "get monitors failed\n");
		exit(101);
	}
```

Replace this with `n = -1`, one `XRRGetMonitors(..., True, &n)`, detached deep
copy, validation, canonicalization, equality, staged no-fail reconciliation,
and one committed-pointer swap. A `NULL/-1` result is failure; do not interpret
it as an empty topology. Runtime failure logs through
`fvwm_debug(__func__, ...)` and leaves pointer, epoch, and legacy view untouched.
Initial failure remains a clear startup failure, matching `FScreenInit()`'s
fatal boundary at `libs/FScreen.c:697-758`.

**Complete event subscription pattern** (expand `libs/FScreen.c:410-415`):

```c
void
FScreenSelect(Display *dpy)
{
	XRRSelectInput(dpy, DefaultRootWindow(dpy),
		RRScreenChangeNotifyMask | RROutputChangeNotifyMask);
}
```

Retain this single subscription function, adding CRTC, output-property, and
resource masks. Root `StructureNotifyMask` belongs in `eventmask.h`, not here.

**Legacy lookup to correct** (`libs/FScreen.c:316-335`):

```c
RB_FOREACH(m, monitors, &monitor_q) {
	if (m->si->rr_output == output) {
		mret = m;
		break;
	}
}
```

Make `monitor_by_output()` inspect every sorted output in the committed
snapshot. Preserve stable existing `struct monitor` nodes while reconciling the
legacy RB tree/TAILQ compatibility view after a changed commit.

**Do not copy:** current `scan_screens()` clears flags before query, mutates nodes
while enumerating, stores only `*rrm.outputs`, and applies synthetic primary
fallback before raw equality (`libs/FScreen.c:585-694`). Those are the exact
anti-patterns this phase replaces.

---

### `libs/FScreen.h` (provider interface, request-response)

**Analog:** existing public control/query declarations at
`libs/FScreen.h:179-201`.

```c
/* Control */
void FScreenInit(Display *dpy);
void FScreenSelect(Display *dpy);
void FScreenSetPrimaryScreen(int scr);

/* Screen info */
Bool FScreenGetScrRect(fscreen_scr_arg *, fscreen_scr_t,
	int *, int *, int *, int *);
```

Include `monitor_topology.h` and add a read-only committed snapshot getter plus
an explicit refresh result API (changed/equal/failed), so `events.c` can run
downstream work only for a semantic commit. Keep publication state private to
`FScreen.c`; callers receive `const` pointers and never free or mutate them.

Do not enlarge `struct screen_info` into a second authority. Its single
`rr_output` may remain a derived compatibility field, while all new consumers
use the snapshot's complete output set.

---

### `libs/meson.build` (config, batch)

**Analog:** existing library source list (`libs/meson.build:1-26,73-79`).

```meson
libfvwm3_sources = files(
    'FScreen.c',
    'ImageSafety.c',
    # ...
)

libfvwm3 = static_library(
    'fvwm3',
    sources: libfvwm3_sources,
    include_directories: includedirs,
    dependencies: all_found_deps,
    install: false,
)
```

Add `monitor_topology.c` to the alphabetized project-owned source list. The
header is not listed. Reuse existing `all_found_deps`; no new package is needed.

---

### `fvwm/topology_refresh_policy.c` (utility / policy, event-driven)

**Analog:** `fvwm/event_loop_policy.c`.

**Pure state-machine style** (`fvwm/event_loop_policy.c:3-17,29-35`):

```c
#include "config.h"

#include "event_loop_policy.h"

int fvwm_bounded_queue_drain(
	fqueue *queue, fvwm_queue_drain_state *state, unsigned int budget,
	fvwm_queue_consumer_t consume, void *context)
{
	if (state->depth == 0)
	{
		state->remaining = budget;
	}
	/* ... */
	return !FQUEUE_IS_EMPTY(queue);
}
```

Copy the explicit state pointer, injected inputs, no hidden clock/global access,
and typed return pattern. The policy unit should receive monotonic timestamps
from its caller and implement:

```text
due = min(last_dirty + quiet_period, first_dirty + maximum_latency)
```

Provide operations for initialize, mark dirty, compute remaining timeout with
positive values rounded up to milliseconds, test due, and complete/reset. Keep
all X calls, `select()`, sleeping, and logging out of this unit.

---

### `fvwm/topology_refresh_policy.h` (utility interface, event-driven)

**Analog:** `fvwm/event_loop_policy.h`.

**Constants/state/status pattern** (`fvwm/event_loop_policy.h:3-34`):

```c
#ifndef FVWM_EVENT_LOOP_POLICY_H
#define FVWM_EVENT_LOOP_POLICY_H

#define FVWM_X_EVENT_BUDGET 64

typedef struct
{
	unsigned int depth;
	unsigned int remaining;
} fvwm_queue_drain_state;

#define FVWM_QUEUE_DRAIN_STATE_INIT { 0, 0 }

typedef enum
{
	FVWM_X_EVENT_NONE,
	FVWM_X_EVENT_DELIVER,
	FVWM_X_EVENT_POLL
} fvwm_x_event_action;
```

Use named, easy-to-tune quiet/max constants (research baseline: 50 ms and 250
ms), an initializer macro, a state struct containing `dirty`, `first_dirty`, and
`last_dirty`, and enum/bool results rather than magic integers. Use a monotonic
time representation that allows exact boundary tests and checked/saturating
addition.

---

### `fvwm/topology_event_adapter.c` / `.h` (event decision and callback seam)

**Analog:** `fvwm/event_loop_policy.c/.h`, supplemented by the explicit callback
context shape in `fvwm/event_loop_policy.h`.

Keep protocol classification and timeout/result decisions as narrow typed
functions with all state passed explicitly. Define one call-scoped dispatch
contract whose callback table contains only update-configuration and dirty-mark
operations. The screen-change path orders update before mark; all other accepted
events mark once; rejected events do nothing. Do not expose refresh,
reconciliation, EWMH, broadcast, pan-frame, or Status callbacks from this seam.
This lets `tests/test_topology_event_adapter.c` prove the exact function used by
`events.c` without linking the full event loop or creating hidden globals.

New `topology_event_adapter.c` includes generated `config.h` first and carries a
GPL notice; the header uses a repository-style include guard and is exempt from
the source-file GPL notice requirement. Format both with root `.clang-format`.

---

### `fvwm/events.c` (event-loop controller, event-driven)

**Analog:** existing RandR dispatch and descriptor timeout integration.

**Imports pattern** (`fvwm/events.c:47-110`):

```c
#include "config.h"
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/randr.h>
/* ... */
#include "event_loop_policy.h"
#include "schedule.h"
#include "builtins.h"
#include "libs/FScreen.h"
```

Add `topology_refresh_policy.h` with the core headers. Keep refresh state static
to the event-loop coordinator rather than adding an unprefixed global.

**Replace immediate per-event work** (`fvwm/events.c:4116-4137`):

```c
switch (e->type - randr_event) {
case RRScreenChangeNotify:
	XRRUpdateConfiguration(e);
	monitor_output_change(e->xany.display, NULL);
	monitor_update_ewmh();
	monitor_emit_broadcast();
	initPanFrames(NULL);
	return;
case RRNotify:
	monitor_output_change(e->xany.display, NULL);
	monitor_update_ewmh();
	monitor_emit_broadcast();
	initPanFrames(NULL);
	return;
}
```

Retain immediate `XRRUpdateConfiguration(e)` for screen changes, but make all
relevant RandR events only mark the policy dirty. Filter `RRNotify` by topology
subtype. Before normal root dispatch, treat root `ConfigureNotify` as dirty to
cover `RRSetMonitor`/`RRDeleteMonitor`; the current core jump table has no
`ConfigureNotify` handler (`fvwm/events.c:4056-4086`), so this check belongs in
`dispatch_event()` rather than a new general handler.

**Run due work at the loop boundary** (`fvwm/events.c:4208-4240`):

```c
while (!isTerminated)
{
	fvwm_process_deferred_signals();
	last_event_type = 0;
	/* deferred window work */
	if (My_XNextEvent(dpy, &ev))
	{
		dispatch_event(&ev);
	}
}
```

Check/run a due topology refresh at the top of each iteration, before taking
another queued X event. On `changed`, run legacy reconciliation followed by the
existing EWMH/broadcast/pan-frame sequence and `status_send()` once. On `equal`
or `failed`, clear/complete the current dirty generation without downstream
work; failure logs and preserves the old snapshot.

**Merge deadlines with existing timeout** (`fvwm/events.c:4315-4351`):

```c
ms = squeue_get_next_ms();
if (ms == 0)
{
	squeue_execute();
	ms = squeue_get_next_ms();
	if (ms == 0)
	{
		ms = 1;
	}
}
/* convert ms into timeout */
```

Compute the topology remaining time and use the minimum nonnegative timeout
with `squeue_get_next_ms()`. Preserve `force_poll` behavior and the existing
zero-time guard. Obtain `CLOCK_MONOTONIC` time in this integration layer, not in
the pure policy.

**Downstream work source:** keep and call the existing helpers at
`fvwm/events.c:1779-1868`; do not duplicate their EWMH, window reassociation, or
module broadcast logic.

---

### `fvwm/eventmask.h` (config, event-driven)

**Analog:** existing root mask (`fvwm/eventmask.h:55-60`).

```c
#define XEVMASK_ROOTW \
	(LeaveWindowMask| EnterWindowMask | \
	PropertyChangeMask | SubstructureRedirectMask | KeyPressMask | \
	KeyReleaseMask | \
	SubstructureNotifyMask | ColormapChangeMask | \
	ButtonPressMask | ButtonReleaseMask)
```

Add `StructureNotifyMask` to this existing macro. Do not substitute broad
`PropertyNotify` refreshes; root `ConfigureNotify` is the Monitor-object signal
required by the protocol.

---

### `fvwm/builtins.c` (inspection controller, request-response + file-I/O)

**Analog:** existing `Status` cJSON serializer and `PrintInfo` subject dispatch.

**Imports pattern** (`fvwm/builtins.c:18-48`):

```c
#include "config.h"
/* ... */
#include "libs/log.h"
#include "libs/cJSON.h"
/* ... */
#include "fvwm.h"
```

The existing `fvwm.h` transitively includes `FScreen.h`; an explicit
`libs/monitor_topology.h` include is preferable if this file directly names
snapshot structs.

**cJSON ownership/error pattern** (`fvwm/builtins.c:132-161,205-215`):

```c
cJSON *msg = NULL, *screens = NULL;
char *as_json = NULL;

if (status_fp == NULL || !Scr.flags.are_windows_captured)
	return;
if ((msg = cJSON_CreateObject()) == NULL)
	return;

cJSON_AddNumberToObject(msg, "version", 3);
screens = cJSON_AddObjectToObject(msg, "screens");
/* ... */
if ((as_json = cJSON_PrintUnformatted(msg)) == NULL)
	goto out;
fprintf(status_fp, "%s\n", as_json);

out:
cJSON_free(as_json);
cJSON_Delete(msg);
```

Bump the top-level version to 4 and add `topology` from the committed snapshot:
`epoch`, then deterministically ordered monitor objects containing name,
name-atom, raw primary, geometry, physical millimetres, and sorted complete
outputs. Build all JSON through cJSON and retain the single cleanup path. Do not
serialize from `monitor_q` or hand-build JSON strings.

**Existing transport** (`fvwm/builtins.c:217-250`): keep `fvwm3.pipe` and
`FVWM_STATUS_PIPE`; this phase adds no service or protocol.

**PrintInfo dispatch pattern** (`fvwm/builtins.c:2632-2675`):

```c
rest = GetNextToken(action, &subject);
/* ... */
if (StrEquals(subject, "Colors"))
{
	PicturePrintColorInfo(verbose);
}
/* ... */
else
{
	fvwm_debug(__func__, "Unknown subject '%s'", action);
}
if (subject)
{
	free(subject);
}
```

Add `Topology` as another subject and render concise text from the same committed
snapshot getter. Follow `fvwm_debug(__func__, ...)` and preserve token cleanup.
No authentication/guard pattern applies; these are existing local FVWM commands.

---

### `fvwm/meson.build` (config, batch)

**Analog:** `fvwm/meson.build:1-56`.

```meson
fvwm_sources = [
    'fvwm/events.c',
    'fvwm/event_loop_policy.c',
    # ...
]
```

Add both `fvwm/topology_refresh_policy.c` and
`fvwm/topology_event_adapter.c` exactly once to `fvwm_sources`, keeping the
two topology units adjacent to the existing event policy source and preserving
the source-root-relative path convention. Plan 03 owns refresh-policy
registration; ordered Plan 04 owns event-adapter registration. Do not list the
headers. The 25-file classification count already includes this shared build
file and remains unchanged.

---

### `doc/fvwm3_manpage_source.adoc` (documentation, transform)

**Analog:** existing command sections.

**PrintInfo pattern** (`doc/fvwm3_manpage_source.adoc:3415-3451`): the command is
introduced as `*PrintInfo* _subject_ [_verbose_]::`, followed by one paragraph
per valid subject. Add `_Topology_` to that list and define that it prints the
committed epoch and complete deterministic monitor facts.

**Status schema pattern** (`doc/fvwm3_manpage_source.adoc:8085-8119`): the current
section documents transport, purpose, and a literal version-3 JSON example.
Update it to version 4, include the canonical `topology` object, and document
identity, pixel geometry, raw `physical_mm`, primary, sorted outputs, ordering,
and epoch advancement semantics. Keep the existing `FVWM_STATUS_PIPE` transport
language.

---

### `tests/test_monitor_topology.c` (test, transform)

**Analog:** `tests/test_image_safety.c`.

**Test structure** (`tests/test_image_safety.c:1-18,82-90`):

```c
#include "ImageSafety.h"

#include <assert.h>
#include <stdint.h>

static void test_image_dimensions(void)
{
	size_t pixels = 0;
	assert(FImageSizeIsSafe(1920, 1080, 32768,
		64U * 1024U * 1024U, &pixels));
}

int main(void)
{
	test_image_dimensions();
	return 0;
}
```

Use one `static void test_<behavior>(void)` per invariant and invoke each
explicitly from `main()`. Cover deep-copy ownership, every structural rejection,
zero/extreme millimetres preservation, monitor/output permutations and
output-set deduplication, one-field-at-a-time inequality, first epoch 1,
equal/failure retention, and idempotent cleanup. If allocation failure is
injectable through an allocator table/context, iterate every failure index and
assert no publication.

---

### `tests/test_fscreen_topology.c` (focused adapter test, request-response)

**Analog:** `tests/test_module_runtime.c` for call-scoped fixture state,
failure-path assertions, and explicit cleanup; retain the plain assertion and
explicit `main()` structure shared by all native tests.

Build deterministic `fscreen_randr_adapter` and `fscreen_legacy_allocator`
fixtures rather than environment/global fault switches. Count query, Atom-name,
release, node/name/screen allocation, and cleanup calls; iterate every allocation
failure index and compare a complete digest of committed pointer, epoch, and
legacy view. The test must compile production adapter behavior directly exactly
once and remain runnable without an X server. Carry the project GPL notice and
format with root `.clang-format`.

---

### `tests/test_topology_refresh_policy.c` (test, event-driven)

**Analog:** `tests/test_event_loop_policy.c`.

**State setup and exact assertion pattern**
(`tests/test_event_loop_policy.c:164-191`):

```c
static void test_x_event_budget(void)
{
	unsigned int consecutive = 0;
	unsigned int i;

	for (i = 0; i < FVWM_X_EVENT_BUDGET; i++)
	{
		assert(fvwm_x_event_policy(1, &consecutive) ==
			FVWM_X_EVENT_DELIVER);
	}
	assert(fvwm_x_event_policy(1, &consecutive) == FVWM_X_EVENT_POLL);
}

int main(void)
{
	test_x_event_budget();
	return 0;
}
```

Inject timestamps directly; do not sleep or call the real clock. Test first
mark, trailing quiet extension, exact quiet boundary, hard cap under repeated
marks, millisecond ceiling, clean/no-timeout state, completion/reset, equal and
failed refresh completion, and a mark arriving during refresh rearming one later
burst.

---

### `tests/test_topology_event_adapter.c` (focused event/callback test)

**Analog:** `tests/test_event_loop_policy.c`.

Use real X11/RandR constants, one `static void test_<behavior>(void)` per case,
and explicit calls from `main()`. Supply fake update-configuration and dirty-mark
callbacks to `fvwm_topology_event_dispatch()`, retain separate counters for
refresh/downstream work, and assert: accepted screen changes trace update then
mark; other accepted sources mark once; rejected roots/subtypes invoke nothing;
refresh/downstream counters remain zero because the callback contract has no
such function pointers. Carry the project GPL notice and format with root
`.clang-format`.

---

### `tests/run_in_linux.py` (platform verification router)

**Analog:** none in the repository. Follow the platform contract in
`01-RESEARCH.md:764-785,787-847`: Linux executes caller argv directly; Darwin
uses Podman `--platform linux/amd64` with the repository mounted as the working
directory; `--live-x` installs the four audited X packages before executing the
argv. Keep caller argv as a list, never interpolate it into the package-install
fragment, preserve exit status, self-test argv construction without launching a
container, use only Python's standard library, and carry the project GPL notice.

---

### `tests/live_x/live_x_harness.py` (test harness)

**Analog:** none; no project-owned Python test/process harness exists.
Use the researched standard-library lifecycle contract from
`01-RESEARCH.md:459-481,686-704`.

```python
read_fd, write_fd = os.pipe()
try:
    server = subprocess.Popen(
        [server_path, "-displayfd", str(write_fd), "-nolisten", "tcp"],
        pass_fds=(write_fd,),
        stdout=server_log,
        stderr=subprocess.STDOUT,
        start_new_session=True,
    )
finally:
    os.close(write_fd)
```

Use argv lists with `shell=False`, private `tempfile` paths and `TMPDIR`,
`time.monotonic()` deadlines, predicate polling, JSON parsing, and reverse-order
`finally` cleanup with bounded terminate/wait/kill. Never use fixed sleeps or
fixed display numbers.

Organize backend-aware scenarios behind one CLI (`--backend=xvfb` and
`--backend=xorg-dummy`). Always require Xvfb startup/RandR/FVWM/epoch-1
inspection; capability-probe mutation by observed `xrandr --listmonitors`
state. Xorg-dummy must not skip after dependency readiness and is authoritative
for create/mutate/remove, server-vs-Status facts, order-insensitive equality,
burst coalescing, and failed-query recovery. Capture xrandr state, last Status
JSON, server log, and FVWM log before cleanup on failure.

---

### `tests/live_x/xorg-dummy.conf` (config fixture)

**Analog:** none in the repository. Follow the proven fixture requirements in
`01-RESEARCH.md:459-481,813-819`: deterministic virtual size and modes, at least
`DUMMY0` and `DUMMY1`, and enough video memory for the selected modes. Keep
runtime output connection, position, primary, and millimetre mutations in the
Python scenario so each assertion starts from explicit queried state.

The server must be launched with this private config, `-displayfd`, and
`-nolisten tcp`. Do not encode a globally fixed display or host X paths.

---

### `tests/live_x/xrr_query_fault.c` (test middleware, request-response)

**Analog:** none in the repository. Implement the research-directed Linux-only
`LD_PRELOAD` interposer (`01-RESEARCH.md:507-509,749-757,813-819`). It should
resolve and delegate to the real `XRRGetMonitors`, fail exactly the requested
next call, preserve the `nmonitors == -1` failure discriminator, and otherwise
be transparent.

Keep the trigger scoped to the spawned FVWM environment and test temp directory.
Do not add a production command, permanent environment switch, or source-level
fault branch to `FScreen.c`. Build this as a test-only shared library; if
interposition proves unavailable in the final Linux lane, the permitted fallback
is a test-build-only linker wrap, not a production hook.

---

### `tests/meson.build` (test registration, batch)

**Analog:** existing C registrations (`tests/meson.build:1-29`).

```meson
event_loop_policy_test = executable(
    'test-event-loop-policy',
    ['test_event_loop_policy.c', '../fvwm/event_loop_policy.c'],
    include_directories: includedirs,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('event-loop-policy', event_loop_policy_test)
```

Register `monitor-topology` and `topology-refresh-policy` as assertion-enabled
native executables. Link or compile the unit under test exactly once; use
`includedirs`, `all_found_deps`, and `libfvwm3` only where needed.

Gate live registrations on the new boolean option. Reuse root `py` and `fvwm3`
(`meson.build:241-252,501-533`). Register Xvfb and Xorg-dummy with `suite:
'live-x'`, hard timeouts, and `is_parallel: false`; pass backend and executable
paths as argv. Build the fault interposer only inside this option gate so normal
and sanitizer matrices do not depend on Linux preload behavior.

---

### `meson.options` (config, batch)

**Analog:** boolean doc options (`meson.options:37-41,55-59`).

```meson
option(
    'htmldoc',
    type: 'boolean',
    value: false,
    description: 'Enable generation of HTML man pages',
)
```

Add a default-false `live-x-tests` boolean with a clear dependency/CI-only
description. A boolean is appropriate because missing X server packages should
not silently alter the authoritative lane; the dedicated job explicitly enables
it after installation.

---

### `.github/workflows/ccpp.yml` (CI config, batch)

**Analog:** existing build/sanitizer job structure
(`.github/workflows/ccpp.yml:13-66`).

```yaml
jobs:
  build:
    runs-on: blacksmith-4vcpu-ubuntu-2404
    container:
      image: ${{ matrix.os == 'alpine' &&
        'fvwmorg/fvwm3-build-alpine:latest' ||
        'fvwmorg/fvwm3-build:latest' }}
    strategy:
      matrix:
        os: [blacksmith-4vcpu-ubuntu-2404, alpine]
        compiler: [gcc, clang]
    steps:
      - name: Checkout repository
        uses: actions/checkout@v5
      - name: Configure and Build
        run: |
          meson setup builddir --buildtype=debugoptimized
          meson compile -C builddir
          meson test -C builddir --print-errorlogs
```

Add one separate, non-matrix, native Debian/Ubuntu container job. Install
`xvfb`, `x11-xserver-utils`, `xserver-xorg-core`, and
`xserver-xorg-video-dummy`; configure with `-Dlive-x-tests=true` (and disable
unneeded optional build work if useful), compile, then run `meson test -C
builddir --suite live-x --print-errorlogs`.

Do not multiply live-X across the compiler/OS matrix or sanitizer job. Add an
`if: failure()` artifact upload for the harness artifact directory; no upload
pattern currently exists in this workflow, so follow the pinned action style
used for checkout and keep artifact paths deterministic.

## Shared Patterns

### Recoverable Validation and Ownership

**Sources:** `libs/ImageSafety.c:7-16,27-38`,
`fvwm/module_runtime.c:82-117,167-243`

**Apply to:** snapshot model, FScreen candidate/query path, fault shim.

- Initialize outputs before work (`module_receive()` sets `*ret_input = NULL` at
  line 176).
- Check bounds before arithmetic/allocation (`module_output_queue_reserve()` at
  lines 108-117).
- Return explicit bool/status values and use one cleanup path where ownership is
  partial.
- Candidate allocation failures are recoverable and must not use terminating
  `fx*alloc` helpers; stable legacy-node setup may keep existing fatal-allocation
  conventions only after all failure-prone staging is complete.

### Runtime Diagnostics

**Sources:** `libs/FScreen.c:717-757`, `fvwm/module_runtime.c:199-242`

**Apply to:** FScreen refresh and events integration.

```c
fvwm_debug(__func__,
	"Module(%p) command is too big (%lu), limit is %d",
	module, size, MAX_MODULE_INPUT_TEXT_LEN);
goto err;
```

Use `fvwm_debug(__func__, ...)` for recoverable runtime query/validation failure,
including enough context to diagnose why the old epoch was retained. Initial
RandR/snapshot failure may terminate clearly at the existing `FScreenInit()`
boundary.

### Single-Threaded Deferred Work

**Sources:** `fvwm/events.c:4208-4439`, `fvwm/event_loop_policy.c:7-62`

**Apply to:** refresh policy and events integration.

Dirty marking is constant-time. One monotonic deadline is merged into the
existing `select()` wait; no thread, signal timer, sleep, subprocess, or file I/O
enters the core event path. Run one synchronous RandR query only when due and
retain existing X/module fairness budgets.

### One Canonical Inspection Source

**Sources:** `fvwm/builtins.c:132-257,2632-2675`,
`doc/fvwm3_manpage_source.adoc:3415-3451,8085-8119`

**Apply to:** `Status` v4, `PrintInfo Topology`, live-X assertions, docs.

Both human text and JSON read the committed snapshot getter. Status remains
newline-delimited unformatted cJSON on the existing FIFO. Tests compare strict
JSON types and facts, not debug text or legacy monitor ordering.

### Native Test Shape

**Sources:** `tests/test_image_safety.c:1-90`,
`tests/test_event_loop_policy.c:45-191`, `tests/meson.build:1-29`

**Apply to:** all four focused C tests: monitor topology, FScreen topology,
refresh policy, and topology event adapter.

Use plain `<assert.h>`, static named cases, explicit `main()` invocation, and
`-UNDEBUG`. Pure units take injected data/time and need no X server. X-visible
behavior belongs exclusively in the live-X suite.

## No Analog Found

| File | Role | Data Flow | Reason / Planner Fallback |
|---|---|---|---|
| `tests/run_in_linux.py` | platform verification router | batch + process execution | No project-owned cross-platform test router exists. Use `01-RESEARCH.md:764-785,787-847` and the Linux-direct/Darwin-Podman contract above. |
| `tests/live_x/live_x_harness.py` | test harness | event-driven + file-I/O | No project-owned Python test or child-process harness exists. Use `01-RESEARCH.md:459-481,686-704,813-839`. |
| `tests/live_x/xorg-dummy.conf` | config fixture | request-response | No Xorg dummy fixture exists. Use the empirically verified backend contract in `01-RESEARCH.md:131-140,459-481`. |
| `tests/live_x/xrr_query_fault.c` | test middleware | request-response | No preload/link-wrap shim exists. Use the isolated test-only design in `01-RESEARCH.md:507-509,749-757,813-819`. |

## Metadata

**Analog search scope:** `libs/`, `fvwm/`, `tests/`, `doc/`, `meson.build`,
`meson.options`, and `.github/workflows/`

**Strong analog families used:** 5

1. Pure validation/model: `libs/ImageSafety.c/.h`, with lifecycle details from
   `fvwm/module_runtime.c`
2. Pure event policy, dispatch seams, and tests: `fvwm/event_loop_policy.c/.h`,
   `tests/test_event_loop_policy.c`, plus call-scoped fixtures from
   `tests/test_module_runtime.c`
3. XRandR/event integration: `libs/FScreen.c/.h`, `fvwm/events.c`,
   `fvwm/eventmask.h`
4. Inspection/docs: `fvwm/builtins.c`, `doc/fvwm3_manpage_source.adoc`
5. Build/verification: `tests/meson.build`, `libs/meson.build`,
   `fvwm/meson.build`, `meson.options`, `.github/workflows/ccpp.yml`

**Pattern extraction date:** 2026-08-29
