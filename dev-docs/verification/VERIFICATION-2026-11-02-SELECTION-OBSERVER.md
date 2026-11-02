# Ordered selection observation and shared Linux transfer checkpoint

Owner Cam; #22/#63/#71/#74; base merged PR338
`627ec305f7fb8e23d7b4fed1584725d3179dd0d5`.
[Finite contract](../decisions/SELECTION-SOURCE-OBSERVER-2026-11-02.md).
Four related stories add pure source lifetime/generation policy, an optional
module-only XFixes observer, bounded queue/token integration and independent
fixtures. This combines the preceding borrowed-transfer batch with the observer;
no installed capture, clipboard default, UI or mandatory dependency is added.

A source token binds the actual display connection and selection to a unique
observer epoch, nonwrapping generation and owner. Subscription precedes a
same-connection query; a complete queued-event scan without ownership changes
is required to mint a token. Authentic changes, same-window reacquisition,
owner loss and incomplete scans revoke it. Unrelated events retain FIFO order.
Missing capability refuses explicitly. The core event loop is not a caller.

## Source and review

Separate preliminary source review found no implementation blocker and requested
one missing genuine-notification refresh case; the fixture now requires RETRY,
zero outputs and generation advancement before clean recovery. All shipped
private C source-current callbacks receive the actual query display/selection;
observer mismatch refusal does not touch its queue or policy. Existing wrappers,
configuration syntax and module packet/Perl metadata remain unchanged.

The host's first pure-test compile failed for a missing stddef include; the
corrected affected compilation passes. Host observer compilation covers the
feature-disabled branch only because XFixes headers are absent. Do not present
it as enabled observer qualification. Python AST, repository/link and whitespace
checks are separate from the native results below.

## Combined native checkpoint

The owned Linux ARM64 profile uses Ubuntu 24.04.5 userspace, GCC13/Clang18 and
the Fedora AppleHV kernel. Frozen source, bounded fresh commands and exact
cached link-closure hashes are retained. Warm source/builds are not modified;
there are no new packages or storage changes. The cached archive supplies common
helpers and dependencies; it does not qualify every source inside it or supply
full sanitizer instrumentation. Public test atoms/patterns on private Xvfb
servers do not access a user's clipboard. Existing nobody owns runtime children;
root-owned frozen source/binaries are read-only to that identity.

The first attempt passes all 38 affected compilations/links and normal pure
policy, property and legacy selection, then fails a native-adapter success
assertion before observer/sanitizer execution. That original result is retained.
Source analysis identifies a pre-poll race: synchronous source fences can queue
a selection reply after the transfer scan, leaving an empty socket to poll until
the deadline. The correction peeks only for this attempt's events after the last
fence/service, without extending deadlines, periodic polling or spinning on
unrelated input. A deterministic no-service fixture withholds the reply until
the first empty transfer scan, then queues it during the pre-poll source fence.
Its synthetic callback performs X operations solely to arrange the regression;
the production observer callback remains queued-only.

The affected-only second attempt reuses exact prior objects/binaries and runs
12 compile/link commands. Normal legacy selection, all content transfer/adapter
cases, the enabled observer and compiled-disabled refusal pass. Actual same-
window ownership has generation2→3; owner A→B→A has generation4→6 before a new
snapshot. Synthetic C-event filtering is not substituted for those server
observations. The negative-capability profile then fails because this Xvfb
aborts with a private-key assertion when XFIXES alone is disabled; its client
exits1 instead of the expected77. That server/configuration failure is preserved,
not a successful capability refusal. The pending sanitizer and bounded corrected
server profile are a separate continuation with no candidate source change.

Both attempts stop and verify the exact owned container before streamed
filesystem collection; independent holder receipts confirm container and machine
stopped. All 90 first-attempt and 40 second-attempt exported files verify against
their recorded hashes. Failure/timeout paths abort subsequent profiles and
quiesce before collection. Successful profiles also require no remaining runtime
UID processes, using actual /proc status UIDs rather than dump-dependent inode
ownership. Resource limits and shutdown receipts do not assert individual Xlib
calls are preemptible.

The third attempt reuses all exact binaries/objects with **zero compiler/link
commands**. Scoped Clang ASan/UBSan pure policy, property, legacy selection,
content adapter and enabled observer pass; its public-X suite takes 5.341 seconds.
The normal corrected suite took 5.085 seconds. These are qualification costs,
not WM performance measurements. The normal pure/property passes from attempt01
remain applicable because their source contracts are unchanged. Later exact
reuse guards validate the prior binaries again; passing sanitizer work has
explicit post-profile source/binary and runtime-UID checks.

The final bounded absence profile also disables DAMAGE and COMPOSITE, then
encounters the same external-server assertion. Those switches are valid runtime
controls in [primary Xserver source](https://github.com/XQuartz/xorg-server/blob/master/mi/miinitext.c);
this was a configuration hypothesis, not a proven root cause or fix. **Runtime
server-absence refusal remains unavailable.** The third attempt's overall result
is failed/retained even though its sanitizer profile passes. No further Xserver
investigation is selected. Preserve compiled-disabled refusal as its own result;
it does not replace the unavailable server-capability case.

All 19 third-attempt exports verify, and both owned resources are confirmed
stopped. [Machine-readable receipts](VERIFICATION-2026-11-02-SELECTION-OBSERVER.json)
retain all three plans/results, exported hashes, exact source, profile rows and
remaining limits. All source changes compile/link with both affected native
compilers. Final integrated and signed-delivery review remain separate.

## Remaining acceptance

Installed capture, allowed transient Xlib copies, lock-time capture/access,
retention/actual consumers, other-target and full release qualification remain
separate. History-retained-while-locked does not authorize capture or reading
while locked. Q1's native owner/control evidence remains accepted; sanitizer
mlock interception cannot establish actual OS locking. The earlier six-target
allocator comparison retains its exact source scope. No new UI exists, so
screenshots/theme checks are not applicable. #257/#258 remain parked.
