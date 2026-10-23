# Bounded directory watching for the native browser

Status: implemented; focused qualification passes in the
[report](../verification/VERIFICATION-2026-10-23-BROWSER-DIRECTORY-WATCH.md).
Owner: FILE-DATA-01, coordinated with FILE-VIEW/ACTION-01.
Base: merged PR173 `be13d246442c0a324f84c6747be268aaa1c3dee1`.

## Batch and resource ownership

Add opt-in native directory notifications, coalesced snapshot reconciliation and
explicit watch/manual-fallback controls to the existing optional Go consumer.
Manual refresh remains the default. `--watch auto` or F8 enables observation;
F8 disables it. Cancel work disables watching as well, preventing an immediate
background retry of work the user just cancelled.

One isolated observer process owns one opened directory plus one native event
queue. Linux inotify watches the observed descriptor through procfs; FreeBSD
and OpenBSD kqueue attach EVFILT_VNODE to the opened directory. Open/register/
stat/read calls stay outside the control loop. There is still one separate
enumeration/action child, one latest navigation and the existing reap ownership.
The observer has one process slot, one bounded event channel and one latest
desired directory; a replacement cannot start until the prior child is reaped.
Cancellation is not a promise that a blocked kernel call terminates immediately.

The private observer protocol carries constant ready/change/loss/expiry states,
never filenames, labels, file content or executable commands. A 16 KiB request
cap and 128-byte response-frame cap bound decoding. Native event names are
discarded. Linux event lengths are validated before traversal; overflow requests
a complete snapshot, not partial replay. The observer emits at most two change
records per second, and the browser admits at most one background rescan per
second. Repeated changes occupy one dirty bit while user work owns the worker.
Watching never cancels or supersedes navigation or an application action.

## Consistency, fallback and lifetime

Ready triggers one complete rescan after registration to close the initial
scan/arm gap. Periodic 30-second reconciliation supplements advisory events:
directory kqueue does not report every child's metadata edit, and filesystem
notification coverage is not universal. Automatic refresh uses the existing
snapshot/selection/native publication contract and preserves input drafts.
Background events do not extend the user's five-minute idle deadline.

An observer has a finite 60-second lease so a quiet orphan exits after parent
loss. A live parent re-arms after reap and reconciles the new ready state.
Startup has a ten-second deadline. Unsupported/failed registration, malformed
or lost transport, directory rename/removal/revocation and refresh failure
switch to explicit manual fallback without retry loops or following a replaced
directory. The previous snapshot remains visible. A successful explicit refresh
or a newly loaded directory can retry an enabled watch; F8 can disable/re-enable
it. Missing Linux procfs is also an explicit fallback. No recursive watch or
per-entry descriptor set is created.

Notifications are invalidations, not an authoritative change journal or a path
lease. Existing isolated action validation remains required. No file mutation,
general files-provider IPC, default desktop switch, mandatory dependency or
protected clipboard channel is introduced. Shared collection policy remains
independent of these filesystem observations.

## Platform references and acceptance

Review uses the native [Linux inotify manual](https://man7.org/linux/man-pages/man7/inotify.7.html),
[OpenBSD kqueue manual](https://man.openbsd.org/kqueue.2) and
[FreeBSD kqueue manual](https://man.freebsd.org/cgi/man.cgi?query=kqueue&sektion=2).
Require actual observer create/change/loss/lease/cancellation tests on Linux,
FreeBSD and OpenBSD, pure admission/coalescing/stale-state tests, affected Go
regression/race/vet/build checks, and the three actual native-X theme drivers
with new watch/refresh/fallback screenshots. Retain full compiler/install/
default-X/release and wider theme/assistive/hardware gates at their boundaries.
