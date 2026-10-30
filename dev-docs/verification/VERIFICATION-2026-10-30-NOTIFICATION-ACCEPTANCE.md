# Native notification acceptance — 2026-10-30

This checkpoint finishes the finite NOTIFY-01 #76 component acceptance around
merged PR310 `bcf6d52d6c5e7e550d0c7a53477c6492b569fba3`. It adds a small
control-size correction and reusable installed/default-X, keyboard and cost
probes. The desktop and remaining R1 services are still in development.

Branch: `codex/r1-notification-acceptance`. The
[machine-readable record](VERIFICATION-2026-10-30-NOTIFICATION-ACCEPTANCE.json)
binds exact input variants, commands, native binaries, original failures and
accepted evidence composition. Final signed commit/tree and separate artifact
review are recorded in the PR.

## Frozen acceptance and scope

| #76 outcome | Evidence composition |
| --- | --- |
| Isolated optional notifications, bounded input/popups/history/actions and replacement/expiry/close | Initial server PR309, lifecycle PR310 and the current installed protocol/UI cases. Only `body` and `actions` are advertised; documented ignored hints and unknown-replacement refusal remain. |
| Live DND/palette/scale and accessible keyboard actions | Earlier actual shared committed-DND projection, actual RandR/strut continuity, three native configurations, and the current four-action traversal, reverse traversal, dismissal, Escape, focus-return and target-size cases. |
| Competing server, selected optional bus and absent/lost service | Accepted exclusive ownership, no adoption/replay, actual stalled startup timer and responsive-core, missing bus, explicit restart and disconnect results; current installed consumers retain these contracts. |
| Malformed/oversized input, churn, callback teardown, slow consumers and installed profiles | Accepted bounded model/decoder, producer churn/action teardown and finite slow-reader results; affected six-target enabled/disabled installed module/manual receipts below. Feature cost is reported separately from a whole desktop. |
| Source-bound policy/real-X, three configurations and affected support | Current per-target policies, nested/default-X consumers, inspected captures and explicit source/profile lineage; original failures remain available. |

The frozen contract asks for usable notification keyboard actions. AT-SPI export
belongs to #57/A11Y-01, physical GPU/input/toolkit/compositor release checks to
#79, secure-locker guarantees to #225, and complete desktop cost envelopes to
#226/#29. Those owners remain open. This clarifies the recent tracker phrase
“physical/accessibility”; historical reports keep their stated limits. None of
those checks becomes passed through issue closure.

## Control correction and independent observations

The default fixed font has a measured height of 13 pixels. Its previous six-pixel
padding produced 19-pixel action rows at 1×, below the existing project floor.
The original installed PR310 binary fails the new pixel observation with five
19-pixel rows. The renderer now uses the greater of font height plus padding and
24 logical UI units. Painting and hit testing share that height; geometry remains
actual pixels on X and follows the existing monitor scale.

The independent XGetImage observer measures five complete horizontal button
keylines, requires the scaled floor and clicks the fourth action's final pixel.
Keyboard cases invoke all four actions, including reverse traversal, dismiss
without an action, and Escape while preserving a live notice. They require the
center to disappear and focus to return to the explicitly ClickToFocus probe
application. The selected state is captured. Existing queued-key, replacement,
capacity, DND, history and owner-loss assertions remain.

The first FreeBSD framebuffer run exposed a fixture assumption: the default
application inherited MouseFocus while the center used ClickToFocus. The strict
return assertion failed after an action. Diagnostic-only instrumentation retained
that assertion and observed the center gone, application/frame alive, pointer
outside every client, active-window zero and the core's no-focus window. That is
normal pointer-focus policy, not a retained center or a reason to add module-owned
previous-focus state. Giving the named synthetic probe an explicit ClickToFocus
policy makes the intended return contract deterministic. No pointer warp or
production focus-policy change was added. The corrected default-X case passes.

The last OpenBSD AMD64 nested check accepted a new notification ID after restart
but did not map its popup. Name acquisition precedes startup DND/configuration,
and a core Echo acknowledges the core command rather than module consumption.
The fixture now sends DND-off and Status on one control stream and waits for an
actual module status with DND off, no pending configuration and a valid monitor
epoch. It bounds attempts, records, bytes and time and preserves partial lines.
Initial and restarted readiness statuses are retained. The original failure did
not collect post-notification signals, so DND suppression is an ordering-based
explanation rather than an observed reason-4 closure. Product ordering is unchanged.
The corrected Linux supplement passes all 12 groups against identical binaries.
The directed OpenBSD AMD64 retry passes 12 nested-X and 12 default-X groups,
including actual startup/restart readiness. Its fresh enabled install and
first disabled/manual checks pass; source, compiler and 17 critical cached
build inputs are verified before and after execution.

## Affected installed support checkpoint

| Target | Actual execution/profile | Installed notification cases | Status |
| --- | --- | ---: | --- |
| Linux ARM64 | Ubuntu 24.04.5 container, Fedora 43 Linux kernel, GCC 13.3; ARM64 CPU via Apple hypervisor | 12 nested-X + 6 Xorg-dummy topology | Passed by explicit composition; final readiness fixture 12/12 |
| Linux AMD64 | Retained Debian 13 Linux kernel/userland, GCC 14.2; QEMU TCG CPU emulation | 12 nested-X + 6 Xorg-dummy topology | Passed |
| FreeBSD 15.1 ARM64 | Native guest kernel, ARM64 CPU via HVF, Clang 19.1.7 | 12 nested-X + 12 authenticated default-X | Passed by explicit composition |
| FreeBSD 15.1 AMD64 | Native guest kernel, QEMU TCG CPU emulation, Clang 19.1.7 | 12 nested-X + 12 authenticated default-X | Passed |
| OpenBSD 8.0 ARM64 | Fresh pinned native guest kernel, ARM64 CPU via HVF, Clang 22.1.6 | 12 nested-X + 12 authenticated default-X | Passed |
| OpenBSD 8.0 AMD64 | Fresh pinned native guest kernel, QEMU TCG CPU emulation, Clang 22.1.6 | 12 nested-X + 12 authenticated default-X | Passed by explicit composition |

This is the affected feature checkpoint, composing unchanged shared support with
the new consumer. It does not repeat the whole compiler/feature/desktop matrix,
claim native AMD64 CPU execution, or rename the Debian profile as Ubuntu. Release
preset, physical hardware and native release sanitizer coverage remain separately
owned. Actual guest/compiler/dependency identities are retained in the JSON.

Each affected target has an incremental build of the current core/MFL,
notification module/client, three notification policies and PNG capture consumer.
A directed retry reuses exact successful compiled inputs and policies rather
than rebuilding them for a fixture change. Tagged
installation supplies the real module and reference manual. Actual notification
consumers execute the installed module with current built core/MFL; this is not
claimed as a fully installed desktop session. The disabled-D-Bus profile has no
notification executable/installed target. Its reference manual intentionally
remains available and matches its actual generated output.

Meson can edit ELF RUNPATH during installation. Provenance applies that target
Meson's exact inspected fixup to a scratch copy and requires full byte equality
with the installed executable; originals are unchanged. The affected Linux
collector first lacked its existing Meson 1.7 API tuple; the inspected compatible
six-argument API is now supported alongside the prior 1.10.2/1.12.1 layouts.
A private PNG build needs both compile discovery and the documented staged
runtime library path. The first fresh disabled profile selected the old system
PNG, and an earlier installed runtime observation loaded it transitively. Those
results are retained with their actual profile; they do not qualify controlled
PNG execution. Correct discovery/runtime paths, loader validation and process
maps bind the qualified private 1.6.59 library. No backport assertion, loader
policy or product dependency declaration was changed.

The optional BSD `--notify-only` driver reuses its authenticated disposable
server and refuses unauthenticated access. CPU declaration is separate from
observed uname; an existing supplied display remains caller-owned. Newly required
labels have defaults for shipped lifecycle/topology callers. The Linux source
review caught that caller mismatch before native execution.

Separate review found that the dismissal no-action assertion inspected only
newly fetched signals after its close helper had retained earlier replies.
The final fixture checks the entire retained queue. A directed offline oracle
checks every actual signal reply from the keyboard-dismiss request through the
original server epoch: closure reason 2 is required and any action for that ID
fails. Wire/result hashes bind each accepted profile. The revised whole fixture
was not rerun; the original native transcripts supply this stronger observation.

## Feature contribution and comparison

The bounded Linux probe records first-ready, settled idle, 32 maximal records
with four actions each, 16 history entries after dismissal, cleared/recovered
state and recovery idle. At observed checkpoints:

| Process | RSS KiB | PSS KiB | Private KiB | Observed kernel VmHWM KiB |
| --- | ---: | ---: | ---: | ---: |
| Notification module | 5532–5784 | 2185–2401 | 1128–1308 | 5532–5784 |
| Private broker | 4240–4244 | 1928–1932 | 1344–1348 | 4240–4244 |
| Core | 5776 | 3069–3105 | 2160–2232 | 5776 |

Both approximately one-second idle intervals contain no protocol/X polling and
observe zero user/system tick increments for those processes. These are finite
observations, not an absolute performance guarantee. First-ready is not continuous
startup sampling; kernel high-water values are observations. Samples are sequential,
not atomic or additive whole-desktop RSS. The broker also serves two independent
fixture peers; a borrowed production bus may already exist. X-server/GPU resource
allocation and external-broker buffering are not process RSS or a release budget.

The observed dependency inventory binds 48 mapped files and 40 installed package
records, with executable/library sizes, hashes, versions and package copyright/
license metadata. Existing native X/font/image dependencies are reused; libdbus
and the independently owned broker retain #77's accepted optional dependency
policy. Per-target installed sizes/linkage supplement this representative Linux
runtime observation. It is not a vulnerability scan or legal audit.

The due [PR305→PR310 core observation](PERFORMANCE-2026-10-30-NOTIFICATION-CORE.md)
uses retained/current binaries and corresponding MFLs. Executable sections fall
about 0.70%; file bytes rise slightly with debug information. Peak RSS is effectively
unchanged and command/timer results are mixed. The profiler does not start this
notification module, so its results are separate from feature contribution above.
The comparison resets the count through PR310 to 0/5; this runtime candidate's
merge becomes 1/5. Current runnable reference binaries are retained.

## Failures, retention and remaining owners

Original target-size, focus-policy, setup, provenance and dependency-profile
failures are preserved. The first directed OpenBSD AMD64 retry failed during
normal package preparation when the remote Ruby archive was truncated; it never
ran the feature gate. Those setup failures remain distinct from a product failure. A private fetch
helper could not run inside normal package privilege separation and was removed.
The replacement prefetches the exact Ruby dependency selected by the normal
solver, with bounded complete gzip/tar validation, then uses ordinary signed
`pkg_add` on the local archive. A prefixed local filename first caused normal
package-name validation to refuse it; the corrected basename matches its actual
package name. Final setup passes. Neither package identity nor signature checks
were disabled; gzip completeness is separate from authenticity. See the
[normal package installer contract](https://man.openbsd.org/pkg_add.1). Historical builds needed a new-option refresh; their
existing retired Cairo command-line option is removed only when it exactly
matches the known prior value. Missing optional D-Bus and documented Asciidoctor
build dependencies were prepared only in owned test guests with normal package
verification. OpenBSD's first documentation-tool setup used an unsupported
package ownership flag; the corrected query follows the
[OpenBSD pkg_info reference](https://man.openbsd.org/pkg_info.1).
No signed-package/trust policy or general dependency requirement changed.

Early review identified that a collector timeout could kill a fixture driver
before its cleanup. The corrected runner allows SIGTERM unwinding and bounded
cleanup, retains partial logs and timeout receipts, and gives the outer BSD
limit more time than the child fixture. A real HarnessRuntime child control
reproduces the old failure and passes with the corrected helper; timeouts remain
failures. This is operational evidence, not a new WM lifecycle feature.

Current source, last useful builds/prefixes/SDKs/caches and runnable comparison
references are retained. Verified duplicate captures/input archives and completed
disposable disks are retired after preservation; owned labs are stopped before
handoff. #57/#72/#77/#79/#225/#226 retain their independent remaining work;
#257/#258 stay parked. Final source review is separate from human approval, and
backend model/effort metadata remains unverified.

The combined `make verify` initially found stale generated Perl command metadata:
`Send_Session` was missing and the `Send_Preview` description predated scoped
resources. Regeneration from the unchanged command definitions refreshes those
two metadata rows; Perl syntax, determinism and exact generated-file equality
pass in the final combined portable check. Native product inputs are unchanged.
The original failing log and final passing log are retained.
