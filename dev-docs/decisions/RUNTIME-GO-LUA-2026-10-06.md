# Runtime language standard: C, Go and Lua

**2026-10-23 update:** the [dependency
decision](DEPENDENCY-SIMPLIFICATION-2026-10-23.md) adds mandatory-Go build
direction, complete first-party Perl/Python retirement ownership and a bounded
Lua engine comparison. The implementation remains queued in Later; the
historical audit and accepted port evidence below remain valid. Earlier
optional-Go and Go-host-only assumptions are superseded for future adoption.

Status: **runtime-first migration direction and scoping approved on
2026-10-06**. The user requested Perl removal, replacement of Python with Go,
and Lua/go-lua where useful, starting with runtime replacement scope. This
records the component audit, ownership, compatibility decision and acceptance
plan. Runtime ports, dependency adoption and removal are not implemented here.

Owners: RUNTIME-GO-01, LUA-01, COMPAT-02 and UTIL-01, coordinated with PROMPT-01,
PERL-PROTO-01, IPC-02, PROC-01, SESSION-01/02, CFG-01/02, FILE-VIEW/ACTION/JOBS,
CONTENT/RETENTION/STORE, XDG-01, TEXT-01, A11Y-01, VERIFY-01 and ARCHIVE-01.
TOOLING-GO-01 records the later first-party tooling migration. The
[tracker](../PROJECT-TRACKER.md#execution-order) owns sequencing.

## Language and compatibility decision

- C11 remains authoritative for WM/X state, the command engine, native rendering,
  established UI modules and existing low-level/resource helpers. A language
  standard is not authorization to rewrite working C modules in Go or Lua.
- Go is the target for first-party runtime services, controllers and installed
  utilities currently written in Python/Perl. Prefer standard-library packages,
  bounded concurrency and reviewed platform adapters; keep native pixel/X and
  protection work with appropriate existing owners. Packaged executables require
  no Python/Perl interpreter or Go compiler at runtime for these functions.
- Lua is the target for optional user-programmable policy and workflows over
  qualified primitives. Shopify/go-lua is the preferred Go-hosted implementation,
  subject to the explicit capability/resource/portability gates below. No
  alternative interpreter is selected implicitly if it fails those gates.
- The user explicitly approves retiring FvwmPerl, perllib and Perl-specific
  evaluation/preprocessing/embedding APIs from the future FVWM-X runtime install.
  This supersedes COMPAT-02's earlier default-preserving optional-Perl direction.
  Arbitrary Perl programs cannot be translated transparently; publish migration
  guidance and precise removed interfaces. Preserve unrelated FVWM syntax,
  FvwmScript, commands, themes, packet ABI and module ordering.
- Standalone utility ports preserve applicable command names, flags, output and
  error behavior through fixtures. Perl-specific framework tooling is retired
  explicitly rather than installing an incompatible executable under its name.
  Do not erase user scripts/configurations or remove another thread's files.
- Runtime first: existing Python Meson/build/test/artwork tooling remains until
  its separate migration. Meson's own Python requirement and external applications
  are not first-party runtime helpers; do not promise a Python-free build toolchain.

## Audited runtime replacements

The current Meson/source inventory is authoritative for installed files. This
table scopes replacements; it does not advertise new component names or syntax.

| Current component | Replacement and retained boundary | Relative scope |
| --- | --- | --- |
| `bin/FvwmCommand.in` (Python) | Go command client with a reusable bounded MFL transport package shared with FvwmPrompt. Preserve socket discovery/CLI; qualify framing, checked writes, incremental JSON, EOF and errors with PROMPT-01/IPC. | Small first port after transport fixtures. |
| `bin/FvwmPrompt` (Go) | Retain Go and its interactive behavior; repair PROMPT-01 before sharing transport. Do not turn command-client reuse into a new server or merge the two public CLIs by inference. | Existing P1 correctness checkpoint. |
| `default-config/desktop.py` / installed FvwmDesktop | Go controller preserving module packets, desktop settings/schema, palettes/scheduling, adaptive bar, menus, metrics, directory browsing, app launch, screenshots and session controls. Keep native Buttons/Script/Form rendering and core window ownership. Measure one resident controller before adding separate resident processes. | Largest port; coherent parity stories and a combined checkpoint. |
| `default-config/launcher.py` | Go discovery/search/ranking, favorites/history, private input channel, result identities, focus-return and native widget coordination. Preserve current Unicode normalization/matching through fixtures; any added text dependency needs explicit review. Optional Lua policies follow parity. | Part of the desktop port, not another daemon. |
| `default-config/session.py` | Go portable capability/action package: OS discovery, explicit argv, backend errors and locker/power behavior. Reuse it for SESSION-01/02 instead of creating another lifecycle owner. | Part of desktop parity; later session expansion keeps its owner. |
| Python screenshot function inside desktop.py | Go orchestration/PNG handling with reviewed native X capture ownership. Current ctypes/Xlib visual/depth/compositor behavior has no Go standard-library equivalent. Review a small C capture worker versus a Go X11 adapter; reuse image/process safety and qualify before choosing. | Separate platform-sensitive story; no automatic screenshot regression accepted. |
| `bin/fvwm-menu-desktop.in` (Python + python-xdg) | Go full menu provider using XDG-01 contracts. Preserve hierarchy, merging, includes/excludes, localization, desktop filters, icon options and installed Form/config interactions. Current Slate app discovery is not a full replacement. Review providers/dependencies and retain literal execution. | Larger utility; independently bounded checkpoint. |
| `fvwm-menu-directory`, `fvwm-menu-xlock`, `fvwm-convert-2.6` (Perl) | Go utilities under UTIL-01 with per-command parity fixtures for traversal, filename bytes, formatting/options, xlock output and non-executing configuration conversion. | Separate ports; do not bundle all utilities into one rewrite. |
| FvwmPerl, perllib, `fvwm-perllib` | Retire installed Perl framework and Perl-only documentation tooling under COMPAT-02 after inventory and documented disposition. LUA-01 replaces selected extension use cases with new APIs; it does not emulate Perl or claim source compatibility. | Intentional compatibility retirement. |
| Script communication example, Form cursor generator, contrib status scripts | Replace useful installed communication examples with Go/Lua examples using reviewed transport; classify development-only generators and optional contrib separately. Update references/install lists/manuals together. Native FvwmScript/Form remain. | Fixture-backed example/contrib cleanup. |
| Native C modules, SVG worker, `fvwm-root`, gzip/system tools | Retain language/ownership; Go calls supported operations or owned subprocesses. Do not replace system lockers, authentication, native decoders or base tools merely to standardize first-party helpers. | Existing owning work. |

Audit references in module configs, installed examples, documentation/manifests,
Perl metadata generation and contrib before claiming removal complete. Historical
material may retain Perl/Python references with clear archival status. Independent
user configurations may still invoke external interpreters; do not police them.

## LUA-01: programming over shared primitives

The initial host is an isolated Go module/service using the existing module/MFL
boundary, with versioned typed IPC-02 operations as they qualify. go-lua is a Go
package, not the native C Lua library. No Go VM, cgo callback bridge or direct
mutable core pointers are embedded into the C WM by this scope. A future native
C Lua embedding would need a separate concrete decision.

| Shared primitive | Useful Lua surface |
| --- | --- |
| CFG/BUTTON and native UI consumers | Register validated policy options, construct bounded module-aware view descriptions, bind known controls/actions, and implement appearance/workflow choices. Native owners draw and enforce settings; this is not a new widget toolkit. |
| FILE-VIEW / CONTENT collection adapters | Produce bounded collection models, grouping/filter policies and contextual action descriptions for files, applications, windows or ordinary clipboard items. Explicit bounded non-sensitive content APIs may support text transformations/snippets after data-flow review. Use snapshots and opaque identities, never display-label identity. |
| IPC / FILE-ACTION / XDG | Invoke named typed operations on validated targets, launch applications with explicit argv through approved APIs and react to correlated results. Window operations still enter the core command engine. |
| FILE-JOBS / SESSION capabilities | React to progress/completion and capability changes; compose actions with explicit user decisions and partial/failure handling. Native/Go owners retain job correctness, authorization, cancellation and service lifetime. |
| RETENTION / STORE and notifications | Apply supported metadata-only retention/display policy and notification presentation. Sensitive plaintext, image derivatives and keys do not enter the Go/Lua heap or general events by default. Any payload exposure requires STORE's separate protection review. |

Required host contract:

- Scripts are explicitly selected local executable configuration, separate from
  private data-only preferences. Directory entries, clipboard contents, remote
  labels and notifications never become scripts. CFG-02 validation must not
  execute Lua or host actions; report dynamic constructs as unchecked.
- APIs have versions, declared capabilities, typed schemas, size/count/depth
  limits and explicit failure results. Use opaque strings/handles for identities
  and generations rather than Lua floating-point numbers for exact 64-bit IDs.
  Preserve filename bytes separately from display text and validate returned data.
- Serialize access to each Lua state on an owner queue. Worker results return
  through bounded callbacks/request IDs; no concurrent goroutine mutation of a
  state or synchronous external I/O in a script callback. Preserve per-producer
  command/continuation ordering and revalidate targets at operation execution.
- Scope subscriptions, timers, operations and handles to script generations.
  Reload validates a replacement before publication, retires old callbacks and
  preserves the last valid UI/config on failure. Define pending-operation policy
  and reentrancy; closing a view cannot free retained content or native resources.
- Expose selected libraries and reviewed host operations only. Do not blindly
  call OpenLibraries: its default set includes filesystem/OS/debug facilities.
  No arbitrary package/native-module loader or implicit shell execution. Any
  explicit trusted command escape must be separately documented and bounded.
- Bound source parsing, callback instructions/deadlines, result bytes, event
  queues and host work. Debug count hooks need tests proving exhaustion cannot
  be defeated by Lua protected calls or repeated recovery. Hooks do not preempt
  blocking Go/native calls or guarantee a per-VM allocation ceiling.
- Go's GC memory limit is soft and its heap is shared with go-lua objects. Use
  qualified process limits/disposable workers where hard isolation is required;
  cancellation cannot mean abandoning an unkillable goroutine. Restricted APIs
  and process isolation alone are not a hostile-code/X11 security guarantee.
- Protect sensitive content in its qualified owner. Go/Lua strings, GC copies
  and automatic wiping are not proof of locked/wipeable secret storage. Keep
  secret workflows handle-based; any secure native boundary preserves leases,
  expiry, representation policy and refusal when protection is unavailable.

## go-lua adoption gate

The upstream README identifies a pure-Go Lua 5.2 VM and documents missing
coroutines, weak tables and other library coverage. Its old performance figures
are not current evidence. The API/source provides protected calls and debug
instruction hooks, but actual limit/error behavior must be exercised. Prefer
short callbacks plus asynchronous host operations; do not promise Lua 5.4,
LuaJIT, coroutine/await behavior or arbitrary Lua package compatibility.

Before adoption, pin/review a revision, license, maintenance/security status,
language/library subset and Go floor. Verify compile-only loading, Lua/Go error
and panic boundaries, stack recovery, numeric/string behavior, reentrancy,
instruction exhaustion through protected calls, parser/allocation floods,
host-call deadlines, reload/generation teardown and a representative model/action
script. First demonstrate a non-sensitive settings/collection consumer. Do not
use a clipboard-secret consumer to qualify the embedding.

Failure of an engine capability gate returns to this owner with the documented
gap; choose a bounded profile or request an explicit engine decision. No new
mandatory native Lua installation is implied by the preferred Go embedding.

## Execution and acceptance

1. Keep current hardening/responsiveness first. At stage 3, repair PROMPT-01 and
   prepare shared Go transport/utility fixtures. PERL-PROTO-01 remains open while
   its installed consumer is reachable; actual retirement may replace its fix
   only with evidence that affected components are no longer installed/used.
2. Interleave small Go client/utility ports at owning checkpoints. RUNTIME-GO-01
   completes the desktop/launcher/session/screenshot parity and Go build/install
   contract in stage 5 before stage 6 desktop services build on those helpers.
   No unqualified default switch or broad unrelated C rewrite.
3. COMPAT-02 completes verified Perl retirement, reference/manual/install cleanup
   and compatibility notices in stage 5. Lua selected-use-case migration follows
   its qualified host; historical/user Perl programs are not translated blindly.
4. LUA-01 establishes its embedding/resource gate and first working example in
   stage 6 alongside CFG/IPC/collection handoffs, then publishes incremental
   qualified scripting surfaces. SESSION-02 bus integration follows stage 7.
   Future exploration is not a prerequisite for the scripting foundation.
5. TOOLING-GO-01 follows runtime qualification with separately scoped build/docs,
   metadata, artwork, tests and maintenance-tool ports. Keep Meson/generator
   dependencies honest and predecessors until their own accepted checkpoints.

Define native module framing/ABI and bounded MFL handling once for Go consumers;
test partial packets, lengths, architecture widths, unknown events, EOF,
backpressure, command ordering and stale generations against independent C peers.
Dependency resolution/build inputs must be pinned and available offline; review
whether pure-Go adapters or narrow native helpers are appropriate per operation.
The current optional Go feature must be reconciled with the future default Go
helpers: select/document the supported default and minimal install profiles at
the build checkpoint, rather than silently retaining Python as a fallback.

Qualification requires the relevant Linux/FreeBSD/OpenBSD AMD64/ARM64 native
policy, real-X, build/install/manual and optional-profile checks. Native Linux
sanitizers still cover C boundaries; Go tests/race checks and Lua fault fixtures
are additional evidence, not replacements. Cross-compilation/target listing does
not prove native BSD/runtime qualification. Measure full-session PSS, startup,
idle CPU/wakeups, peak memory, GC and input latency under load against the Python
baseline; Go adoption does not establish a smaller footprint automatically.

Scope verification on 2026-10-06: audited current Meson install lists, Python
controller/imports, Perl framework/utilities/examples and existing Go prompt.
Local Go 1.27.0 reports all six target combinations via `go tool dist list`;
no port, cross-build, engine execution or native target qualification was run.
Documentation references/anchors and tracker ownership are checked locally.

## Primary references reviewed

- [Shopify/go-lua](https://github.com/Shopify/go-lua): embedding purpose and
  documented language/library limitations; no current benchmark inferred.
- [go-lua API](https://pkg.go.dev/github.com/Shopify/go-lua) and
  [source](https://github.com/Shopify/go-lua/blob/main/lua.go): protected calls,
  number/identity representation and VM API.
- [Library initialization](https://github.com/Shopify/go-lua/blob/main/libs.go)
  and [debug hooks](https://github.com/Shopify/go-lua/blob/main/debug.go): selective
  libraries and instruction-hook surface, requiring qualification.
- [Lua 5.2 manual](https://www.lua.org/manual/5.2/manual.html): reference semantics;
  the selected engine's supported subset must be documented separately.
- [Go GC guide](https://go.dev/doc/gc-guide): memory/CPU tradeoffs and soft limit;
  this is not a per-script hard resource-control mechanism.
