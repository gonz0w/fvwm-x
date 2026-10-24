# M2.1 desktop acquisition and action workers

Status: **focused checks passed; ready for PR review**. This is the first M2
runtime-controller slice, Refs #23/#7/#15. Those broad owners remain open.
[Contract and remaining work](../decisions/M2-DESKTOP-WORKERS-2026-10-23.md),
[source-bound receipt](VERIFICATION-2026-10-23-M2-DESKTOP-WORKERS.json).

## Result and boundary

Move application discovery, Linux/BSD metrics, audio and session power requests
from Python into one private nonresident Go worker. Requests/results are typed
bounded data; the existing resident controller alone publishes module commands.
The bridge/executable own partial I/O, child/group lifetime and bounded backend
output. No privileged helper, shell-command service, new GUI toolkit or native
module protocol is added. Existing FVWM3 settings, bindings and explicit Run
command/shell behavior remain usable.

The launcher retains user-hidden masking, desktop filters, literal argv and
field codes, working directories and terminal flags. Shared Desktop Entry
syntax rejects malformed/duplicate keys, NUL and invalid UTF-8. Launcher POSIX
word quoting stays separate from the full XDG menu's stricter Exec grammar;
no shell expansion occurs. Standard Go executable lookup also refuses implicit
current-directory lookup. Enumeration/results and whole-job budgets are explicit
in the manual; a failed refresh does not replace published data with a partial
list. Existing configuration/schema formats and native semantics do not change.

The controller, private settings storage, previews/editors/configuration checking
and ctypes/Xlib/PNG screenshot path remain Python. Their named source owners
are hash-equal to merged M1. Artwork generation remains developer/build tooling;
Perl framework retirement remains M6. This is not a Python-free desktop or
closure of the full runtime owner. The transitional bridge is deliberate;
no total source-size reduction or Go performance benefit is asserted.

## Combined focused checks

| Gate | Result |
| --- | --- |
| Linux ARM64 / FreeBSD ARM64 Go worker + reused Desktop Entry policy | 50 top cases / three packages each |
| Linux scoped Go race | 10 top cases / two packages; actual race worker exercised through client |
| Existing desktop settings/UI/checker policy | 71 per target, 142 total |
| Original worker/client and installed checks | Nine per normal profile/stage, plus Linux race client: 45 total |
| Final bridge/launcher review checks | 12 cases on each final Linux/FreeBSD installed pair |
| Actual X | Linux full desktop/settings/operations plus FreeBSD settings/operations: five original runs; affected operations-only review repeats pass |
| Installs | Fresh tagged controller/worker pairs on both targets; final stages and adjacent lookup without override pass on both targets |
| Cross-builds | All six Linux/FreeBSD/OpenBSD ARM64/AMD64 pure-Go workers compile offline; these are not six native executions |
| Portable repository suite | 181 pass / 15 environment skips out of 196; final links/lint checked separately |
| Documentation | Changed FvwmDesktop manual renders once |

Each combined normal/race Go set has **110 top-case executions / eight package
outcomes**; final review repeats that affected set after the lexer correction.
Independent peers exercise malformed/duplicate fields, unknown actions, missing
backends, typed counter bounds/overflow, user masking, bounded application scans,
literal audio/power argv, backend pipe inheritance and overflow, blocked request
stdin, oversized/invalid responses and descendant retirement. Power tests use
mock backends and never request a real system shutdown. The preceding Python
acquisition-only cases move to explicit independent Go/CLI expectations; all
71 other desktop policy tests remain. No candidate generates its expected values.

The full Linux native-X fixture exercises actual application launch, search,
calendar, settings/schema/editor/reload/restart and preview workflows. Existing
palette/appearance changes, bar sizing and screenshot RGB at 16/24-bit depths
also pass, with no unexpected observer X errors. Native settings/operation runs
check worker publication, request generations, busy/cancel/timeout ownership and
module responsiveness. Captures of existing UI are retained from that run;
no new rendering needs another three-theme matrix. Core/MFL/Form/Script binaries
and config.h retain their exact identities; no new C/sanitizer qualification is
claimed. The FreeBSD guest shuts down normally. After canonical host verification, 97
Linux and 44 FreeBSD capture files/duplicate archives are retired with exact
hash guards; sources, incremental builds, installed stages and VM disks remain.

The combined Linux commands total about 60 seconds, FreeBSD about 28 seconds.
A five-second bridge/manual/install completion and short operations repeat cover
the setup correction. Final lexer/bridge/installed-operation checks take about
seven command seconds per target. Broader UI and policy evidence is retained;
there is no per-story/full-OS matrix repeat.

## Review correction, provenance and measurements

The first 2376-input freeze passes both combined runs. Review finds a setup-order
gap: spawning before selector allocation can leave a child when allocation fails.
An independent mock control reproduces that ordering. Allocate the selector
first, close it on fork failure, and retain leader ownership through cleanup.
The 11-case bridge completion and affected operations consumer pass.

A second independent control catches the launcher's quoted-backslash mismatch:
Python shlex preserves a backslash before dollar/backtick inside double quotes.
Correct the private lexer, retaining literal characters without shell expansion,
and add Go vectors plus a CLI regression (including nonbreaking-space input).
Final 12-case installed bridge suites, normal Go policy/vet, Linux scoped race,
operations consumers and adjacent lookup pass on Linux and FreeBSD. Six final
workers cross-build offline. Original receipts, both before controls and exact
source deltas remain canonical; none is erased. Settings/capture owners and
native C binaries remain exact. Documentation follows without a product repeat.

Seven alternating paired Linux status acquisitions include new worker startup.
Median baseline is **0.113 ms**, worker **1.058 ms**: about **0.945 ms additional
latency per asynchronous acquisition**. This is an isolation/maintenance port,
not a speedup. The measured executable is 4677354 bytes; this does not describe resident
session memory. This observation predates the launcher-only correction; the
status/supervision implementation is unchanged. Final binary identities/sizes
are recorded separately in the receipt. The raw completion job's aggregate child high-water is not a
standalone worker RSS measurement. Complete startup/RSS/idle CPU/wakeups/input
latency comparisons remain mandatory before #23 closes. Periodic browser
performance is not due: pre-cleanup PR186 reference is 2/5 after merged PR189,
3/5 on this implementation merge. Tuning remains deferred.

Base is merged PR189 `3df09ef2ba32fa0d4c9c517c1e74def012ac5b26`.
The receipt binds source archives/manifests, final consumer hashes, retained
owner-function/native-binary hashes, exact commands, stages and cross outputs.
All canonical archives pass gzip EOF/CRC, payload hashes/modes and source checks.
Local diff/reference/manual review and author-DCO signed commits complete the
PR; Actions remains disabled. CPU-emulated, default-server, physical/provider
and whole-release acceptance are reported separately and are not inferred from
these focused results.

Next: remaining resident controller/settings/launcher coordination, then the
separately reviewed screenshot/native-X boundary. Full M2 handoff qualifies the
installed Python-free desktop before advancing to dependency closure; this PR
does not prematurely mark #23/#7/#15 Done or resume new features.
