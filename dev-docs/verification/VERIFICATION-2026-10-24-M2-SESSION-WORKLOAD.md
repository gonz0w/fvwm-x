# M2.9: bounded session workload and acceptance handoff

PR201 is merged as `28324d119e4f3d261984ec9a8cc8cf9341d8c297`, with canonical
signature, author DCO and exact PR tree verified. Its completed local/remote
branch is retired. This checkpoint changes only acceptance tooling/docs; all
1,185 runtime/build inputs and PR200 installed/native identities stay exact.

The earlier resident GC run used one fixture application and settings dialogs;
it observed no cycles. The new opt-in `--workload-observe` gives the existing
installed controller a fixed natural allocation workload: 512 additional apps,
96 private HOME directories and eight discovery/search/files rounds. There are
513 DesktopEntry files; each discovery must return the existing maximum of 512
records. Each operation must complete through its correlated provider/ID, and
each search/files menu must acquire/release its actual X grab. Three existing
settings-editor cycles/twelve queries and twelve final key/Echo samples remain.
Eleven pure cases cover bounded fixtures, collisions, completion/failure,
identity/counter validation, explicit busy refusals, reply deadlines and callbacks.

## Focused observations

One plain run and one separate `GODEBUG=gctrace=1` run pass on the same unchanged
installed prefix. Inputs and eight-round counts were fixed before execution;
there is no forced collection, GOGC/GOMEMLIMIT adjustment or load escalation.
Both runs complete all eight discoveries and sixteen menu grabs/dismissals,
with complete private cgroup/trace accounting, no observer limit/error or
unexpected X error, successful core exits and journaled resource cleanup.
The native affected pure fixtures and `make verify` pass. No rebuild, repeated
baseline comparison, sanitizer or OS matrix is selected for these test changes.

| Observation | Plain workload | Separate resident trace |
| --- | ---: | ---: |
| Completed application discoveries | 8 | 8 |
| Result records per discovery | 512 | 512 |
| Pre-input operation snapshots | 8 running | 8 running |
| Admission key/Echo median, ms | 3.142 | 3.675 |
| Admission key/Echo range, ms | 2.379–10.778 | 2.976–10.096 |
| Observed resident GC cycles | Uninstrumented | 8 |
| Largest first+final GC clock-phase sum, ms | Unknown | 2.700 |

These are single workload observations, not a paired performance result or a
latency threshold. The environment is Debian13 Linux6.12.113 AMD64 under QEMU
TCG, not native-CPU or Ubuntu release qualification. A `running` snapshot precedes
input and does not prove the worker remained active when input arrived. XTest
to core Echo excludes physical input and completed painting. Instrumented
timings are retained separately and establish no GC-to-input causal correlation.

All eight parsed resident collections are natural; raw first/concurrent/final
clock phases remain in the machine report. The largest individual first/final
phase is 2.1 ms; the largest per-cycle sum is 2.7 ms. Neither is a worst-case
pause bound, and worker stderr is outside this scope. PR201's zero-cycle result
remains valid for its different smaller workload. Sampled memory and charged
cgroup peaks retain their distinct meanings; no physical-memory claim follows.

Four existing-menu images are captured and inspected. Both search captures and
the GC-run files capture show readable controls and fixture entries. The plain
files capture caught an unsettled blank menu and is retained without a rendering
pass claim. No extra load or screenshot run follows that cosmetic limitation.
No product layout/theme changes are made; earlier source-bound theme checks
retain their original scope. Canonical images/raw observations are under
`builddir/codex-2026-10-24/m2-session-closeout/`.

## M2 acceptance handoff

| Requirement | Exact existing evidence | Remaining boundary |
| --- | --- | --- |
| Remaining installed runtime controllers and predecessor retirement | [Resident controller](VERIFICATION-2026-10-24-M2-RESIDENT-CONTROLLER.md), [runtime/session inventory](VERIFICATION-2026-10-24-M2-SESSION-RUNTIME.md) | No additional installed controller/action port found; M6 Perl and M7 build/test tooling stay separate. |
| Settings, private commits, generations, native editors/preview and typed replies | [Resident controller](VERIFICATION-2026-10-24-M2-RESIDENT-CONTROLLER.md), [platform handoff](VERIFICATION-2026-10-24-M2-PLATFORM-HANDOFF.md) | Existing qualified scopes retained; no new preference/UI feature. |
| Native capture, PNG-disabled refusal and installed helper lookup | [Native capture](VERIFICATION-2026-10-24-M2-NATIVE-CAPTURE.md), [platform handoff](VERIFICATION-2026-10-24-M2-PLATFORM-HANDOFF.md) | Hardware/compositor coverage remains separately owned. |
| Six-target affected policy/build/default/reduced/install and BSD default-X | [Platform handoff](VERIFICATION-2026-10-24-M2-PLATFORM-HANDOFF.md) | Selected controller family; not fresh full WM/compiler/release qualification. |
| Interpreter-free installed desktop/utilities and safe locker/power/restart/logout | [Runtime/session](VERIFICATION-2026-10-24-M2-SESSION-RUNTIME.md), [fresh installed acceptance](VERIFICATION-2026-10-24-M2-SESSION-ACCEPTANCE.md) | Safe peers are not authentication, real power or actual login acceptance. |
| Startup, PSS/RSS, charged peak, idle CPU/task wakeups and synthetic input against Python | [Three alternating pairs](VERIFICATION-2026-10-24-M2-SESSION-ACCEPTANCE.md) | One emulated AMD64 environment; higher Go wakeups and overlapping CPU ranges remain visible for later tuning. |
| Natural resident GC and input during representative activity | This fixed workload and [machine receipts](VERIFICATION-2026-10-24-M2-SESSION-WORKLOAD.json) | Eight observed cycles; worker traces, worst-case pauses and actual input/paint remain outside scope. |
| Native-CPU/full-session owner acceptance before M3 | Pending; this checkpoint supplies the runnable workload/evidence map | Requires an owner-controlled native Linux environment and owner review; current resource metadata/available QEMU accelerators do not prove qualification. |

For the remaining native session check, use the same qualified installed prefix
and source-bound observer on an owner-controlled native Linux environment:

```sh
python3 tests/live_x/desktop_session_live.py --observe-only --workload-observe \
    --prefix /path/to/stage/usr/local --version VERSION \
    --artifacts-dir /path/to/new-plain-session
python3 tests/live_x/desktop_session_live.py --observe-only --workload-observe \
    --gc-observe --prefix /path/to/stage/usr/local --version VERSION \
    --artifacts-dir /path/to/new-gc-session
```

Optional existing writable `--cgroup-parent`/`--trace-parent` retain the helper's
private resource limits. Record kernel/architecture, CPU/virtualization facts,
source/installed identities and raw results; absent kernel counters remain
unknown. Do not install dependencies, change host policy, force GC or claim a
native qualification merely because a command recipe exists. Full-session
owner acceptance and separate native sanitizer/release/physical/toolkit/
compositor/assistive requirements are not waived. M2 stays active before M3.
GitHub #23's manual closure is preserved; #7/#15 remain open.

## Integrity, review and cleanup

The [machine record](VERIFICATION-2026-10-24-M2-SESSION-WORKLOAD.json) binds the
frozen source, executed commands, archive hash/size and observations. Full gzip
CRC and every captured payload's hash/size/mode verify before retirement;
source upload payload hashes/modes also verify. Retire only 34 completed guest
capture/archive duplicates (30,442,009 bytes), preserving canonical host evidence,
source/build/cache/install stages/disks and other owners. Both owned kernel
journals confirm cleanup and resource paths absent; the VM powers off normally
with QEMU status zero.

Separate source review resolves duplicate command-prefix and capture-label
integration mistakes before freezing. No executed test logic changes afterward;
only docs/status/handoff additions follow. Requested analyst/implementer/reviewer
model settings are unverified because observed model/client metadata is absent.
Agent review does not constitute independent human approval. The production
performance-comparison counter stays 4/5 after PR200: these observer-only PRs
leave installed binaries unchanged, so no independent browser-scan repeat/reset.
Stop at this bounded handoff; do not create successive observer expansions or
start production tuning/M3/M6/M7 without their owning scope.
