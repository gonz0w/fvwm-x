# Session providers and power coordination — 2026-10-29

Owner: Cam; R1 Major checkpoint under #72/#77 and a bounded #225 consumer.
Base: merged PR305 `8402e098c0fe3fcab8b14f2ff9c52346f7ff7d9b`.
The [machine-readable record](VERIFICATION-2026-10-29-SESSION-PROVIDERS.json)
binds commands, failures, cases, source closures and actual binary identities.

## Delivered checkpoint

Eight related stories share one parent lifecycle: fixed native/login1 discovery,
per-action permissions, guarded poweroff/reboot, retained manual-lock ownership,
explicit secure-suspend refusal, one shared power preference, native/public
consumers and independent bounded loss/reprobe behavior. The
[decision](../decisions/SESSION-POWER-PROVIDERS-2026-10-29.md) records the finite
provider contract and remaining work.

Settings schema 6 reads existing saved schemas automatically, exposes fifteen
preferences and retains the eleven-field appearance editor. Session editing has
four fields. Public Session schema 2 adds typed Power/Locker observations without
changing the five-item service inventory. Public settings identity is carried
once in its envelope; standalone native records retain their own identity.
Private owner protocol version 2 changes producer and consumer together. An old
running parent needs a fresh login after upgrade; configuration syntax remains
usable. Public 8 KiB and private 16 KiB ceilings remain unchanged.

Portable power uses the shared literal systemctl/shutdown policy; discovery
never submits an action. Optional login1 uses the existing isolated libdbus
worker and only an existing local system bus/service. Recheck bus, unique owner
and explicit yes permission before a noninteractive request. Seven known
permission states remain distinct; unknown is nonauthorizing. No automatic
fallback or replay follows a submitted or uncertain request. Request success is
not a completed shutdown.

Manual Lock retains exact xlock/slock child/Wait ownership while the parent
lives, across requester loss and Desktop/core restart. WM exit leaves the locker
running; later original-parent reaping is not attested. Running never establishes
secure protection. Coordinated suspend refuses before any
provider method without a qualified readiness witness. No mandatory package,
new authentication implementation, system-bus creation, service activation,
inhibitor FD loop or arbitrary command registry is introduced.

## Focused results

| Check | Observed result |
| --- | --- |
| Incremental build | Wrapper, Desktop/worker, optional helper, MFL/Form, provider fixture, selected policy and three manuals pass; initial affected build 1.301 s, affected correction 0.408 s |
| Go ordinary/race | Seven packages compose 472 test/subtest passes in each mode; controller-only correction contributes 281 in each mode |
| Go vet | Initial seven-package pass and affected controller repeat pass |
| Portability | FreeBSD/OpenBSD ARM64 wrapper/Desktop cross-builds pass at source01; compile-only, before the platform-independent public projection correction |
| C grammar | MFL request policy passes normally and under ASan/UBSan |
| Optional C power sanitizer | Six private-bus scenarios pass GCC ASan/UBSan: success, method denial, challenge, post-send stall/loss and capability denial, including stale-owner refusal before mutation; distribution libdbus is uninstrumented |
| Default-disabled helper | Probe/request explicitly return unavailable and actual linkage has no libdbus |
| Install/manuals | Authoritative Meson installed destinations supply the private integration prefix; session-startup tagged staging and three manuals pass; corrected MFL manual rebuild passes |
| Native portable | Fixed literal power requests, nonzero-result/no replay, shutdown fallback and absence pass at source01 |
| Native locker | Same-child duplicate refusal, requester loss, Desktop/core restart, WM exit retention and observed child release pass at source02 |
| Native login1 | Seven permissions, stale identity, owner replacement, denied/challenge/stall/loss, no replay, bus reconnect, shared policy and zero Suspend calls pass at source02 |
| Native UI | Slate, Dune and enlarged Classic each pass real Session/menu/settings, stale confirmation refusal and one explicit harmless confirmation submission; nine UI cases |
| Repository | Maintenance check, whitespace and fixture syntax pass locally |

Nineteen selected native cases compose the portable source01 result with the
corrected locker/login1/three-theme source02 results. Passing unaffected checks
are reused. No full OS matrix runs solely for this PR. The focused gate does not
qualify BSD service execution, live systemd-logind/elogind authorization, physical
authentication/lock, idle/lid/display-power/inhibitors or whole-session costs.
Existing startup, optional notification and cooperative logout evidence remains
in the [prior checkpoint](VERIFICATION-2026-10-29-SESSION-SERVICES.md).

## Review and retained failures

Separate source review found a probe-generation race: pending probes could expose
old capabilities under fresh tokens. Admission now clears stale capabilities;
completion rotates provider and parent operation tokens before adoption. Entropy
failure does not adopt a result or invent a token. Regression source covers
pending/intermediate/final tokens and both entropy failures.

The initial fifteen-setting result was 8,632 bytes. Removing duplicated item
owner/generation preserves all fifteen preferences and remaining metadata within
the unchanged 8,192-byte assertion; normal/race tests pass after correction.
The original locker test expected a different admission shape despite the real
failed/busy result and same running child. Its correction requires that terminal
refusal, unchanged locker identity and exactly one child. UI setup raced an
asynchronous generation update; bounded retries now follow only explicit stale
rejection, never accepted mutations or uncertain transport outcomes.

Review also found a blind spot in the negative Suspend assertion and a fake-
locker cleanup race. The private fixture now records rejected method names;
assertions detect Suspend reaching it. A persistent fixture release sentinel,
bounded same-process exit observation and preserved cleanup errors precede
runtime removal. Final native results pass. One gate script omitted Meson's PATH
after the successful build/Go repeat; only the remaining native phase resumed.
All initial failures and the partial script result remain in raw evidence.

The final annotated candidate receives a separate artifact/evidence review in
the PR handoff. Agent review is not human approval; backend model/effort metadata
remains unverified. GitHub Actions remains disabled.

## Screenshots, cost and retention

[Three inspected original captures and configurations](../../doc/development-preview.md#session-power-providers-and-manual-lock)
show Slate status, Dune settings and enlarged Classic confirmation with explicit
legacy Form colors/borders. Diagnostic text intentionally elides; controls remain
contained. The [manifest](../../doc/screenshots/session-providers-manifest.json)
records image/configuration hashes and actual inputs. Whole-guest panel counters
are not WM benchmarks; no real power or authentication is performed.

The current unstripped wrapper is 5,237,967 bytes (+16,387 from PR305), Desktop
6,577,895 bytes, MFL 453,504 bytes and optional helper 189,824 bytes. These are
artifact sizes, not resident memory. The due
[PR299→PR305 core comparison](PERFORMANCE-2026-10-29-SESSION-CORE.md) uses retained
binaries from before this batch. It resets the cadence to 0/5 at merged PR305;
this candidate's runtime merge will be 1/5. No whole-default-desktop comparison
is claimed.

All 513 raw evidence files verify against the 2,097,981-byte host archive,
SHA256 `96cd81e2fb1e7f2f728c68a86191bb25bc84f14b6a33f65cbe81465cf8297d0c`.
Retired 72,458,115 bytes of owned guest duplicates and superseded core/Script/
Form references. Current source, reusable normal/sanitizer builds, scoped
instrumented helper, installed prefix, SDK/caches, next performance reference and
the still-current browser component reference remain. Owned fixture processes
are absent; the Linux container/VM stopped normally.

#72/#77/#225 remain open for their frozen broader acceptance. Persistent
inhibition, qualified secure readiness and idle/suspend coordination, named
agent/portal/tray adapters, stalled-WM recovery and supported-target/release
acceptance retain their owners. #257/#258 remain parked.
