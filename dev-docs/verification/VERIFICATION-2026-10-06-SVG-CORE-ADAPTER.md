# Core image readiness and owner preparation — 2026-10-06

Owner: IMAGE-01 / INPUT-01. Status: implemented with focused preparation passes;
combined activation and full qualification remain pending. Core startup does
not install the optional image loader. Production SVG calls still wait
synchronously. This report does not claim improved running-WM SVG responsiveness,
installed parity or default BSD X qualification for asynchronous images.

## Completed work

- Added the core adapter and ordinary readiness/deadline integration. Normal
  dispatch retains select when there are no worker descriptors. Active decoder
  pipes use poll without putting descriptors above FD_SETSIZE into fd_set.
  A decoder-only deadline cannot trigger the startup-module timeout.
- Nested Wait and interactive Move/Resize service transport only. Starting jobs,
  converting pixels and committing owners require the outer HandleEvents
  boundary, after scheduled destruction and before window-update flushing.
  Interaction polling preserves its error before decoder servicing changes errno.
- Added termination-aware fvwmPoll using the existing signal-jump contract.
  Owned X events/errors go through recording filters, not owner callbacks.
- Wired shutdown while the child reaper remains installed. X connection loss
  cancels the worker without issuing X requests. Inherited state in forked module
  and Exec children cannot service, destroy or cancel the parent's manager.
- Picture rendering skips absent first-load pictures. Colorset cleanup does not
  alter pixmaps borrowed through a picture view.
- WindowList holds an immutable mini-icon backing snapshot instead of sharing a
  mutable view. Its reference remains usable after the view changes or disappears;
  ordinary cached pictures retain their existing reference-count behavior.
- Mini-icon changes prepare before releasing their old cache reference.
  Pending views retain the old backing and stay out of mini-icon broadcasts.
  A real module packet observer confirms three unchanged-style reloads retain
  the same pixmap, rather than reconstructing the image. Ready callback routing
  remains pending with the other image owners.
- Fixed duplicate release of a picture rejected as a colorset shape. An ordinary
  opaque RGB PNG reproduced the invalid lifetime in the real core under ASan.
  Both uncached refusal and refusal of an image shared with a background now
  release only the acquired reference.

## Focused verification

The candidate overlays 21 owned files onto the accepted asynchronous foundation,
with scoped core/test build-graph additions. Final Linux normal/sanitizer and
OpenBSD ARM64 source trees match all 1230 frozen hashes and modes. The 21 root
sources match. The final archive SHA256 is
`095a686d1633f6f375844be7a3c8228a4d3d6658129aeba82c4cac55e2ed7502`.

Evidence is under `builddir/codex-2026-10-06/svg-core/`: `source-current.tgz`,
source/mode manifests, `owned-files.json`, `focused-results.json`, three evidence
archives, and private freeze/collection scripts. These preserve concurrent work
outside the candidate. Successful checks from unchanged inputs are retained;
changed inputs and discovered fixture failures received affected reruns. Source
manifest parity does not imply every suite was rerun on the final archive.

| Focused scope | Result |
| --- | --- |
| Linux ARM64 Clang affected core/library/MFL and image test builds | Passed |
| Normal Linux focused checks | Eight distinct entries passed: scheduler dispatch, interaction servicing/cancellation, ARGB rendering, picture scheduler, readiness unit, core adapter probe and actual image-owner fixture |
| Scoped local Clang ASan/UBSan | Four distinct entries passed: ARGB rendering, readiness unit, core adapter probe and actual image-owner fixture |
| OpenBSD ARM64 early portability | Readiness/signal unit compiled and passed; no core or X adoption qualification claimed |

The readiness unit covers independent base read/write readiness, EOF, invalid
base/worker descriptors, finite/infinite deadline selection and rounding,
termination before/during polling, and preserving select for the inactive path.
Linux also exercises real pipe descriptors above FD_SETSIZE. OpenBSD's login
class has a hard limit below FD_SETSIZE: its real pipe checks stay within that
limit, with the high-descriptor poll contract covered through the controlled
poll wrapper. The test does not raise the administrator's hard limit.

The core adapter probe runs on 24- and 8-bit Xvfb. It checks no worker start from
nested servicing, independent readiness during the fixture worker delay,
transport completion without owner mutation, deferred commits without spinning,
fork-child isolation, no-X fatal cancellation, shutdown handoff and final reap.
This exercises the adapter with a synthetic typed-output worker, not actual
enabled WM resource owners. ARGB checks cover 24-bit, 8-bit and strict 8-bit
rendering, absent draws and retained snapshot lifetime.

The actual-core owner fixture checks two opaque-shape refusals, eight valid
shape/clear/background reload sequences, three unchanged-style mini-icon pixmap-identity checks, and four WindowList
popup/dismiss and mini-icon reload sequences. Probe sanitizer checks enable leak detection; the
actual core/MFL fixture disables leak detection while keeping ASan/UBSan error
checks enabled. Native Linux release sanitizer CI remains a separate unrun gate.

## Failures retained and corrected

The baseline shape control stopped at the first ordinary opaque-image refusal
with a heap-use-after-free in SafeDestroyPicture. Source/binary hashes, ASan
diagnostics and the fixture failure are retained in the sanitizer archive.
The corrected core completes the same refusal and the subsequent shared-image
and reload checks.

The first OpenBSD unit assumed its hard descriptor limit exceeded FD_SETSIZE;
that fixture assumption was removed. The first extended WindowList fixture
allowed interactive client placement and timed out before reaching WindowList.
Explicit TileCascadePlacement makes placement deterministic. Timeout logs were
preserved, its six verified fixture processes were stopped, and the corrected
normal/sanitizer owner checks passed. These were fixture failures, not evidence
of a WindowList rendering regression.

## Remaining work

Colorset background/shape preparation, decor and per-window scale variants,
general menu/icon completion, cursor/root-tile ownership, and callback routing
must be completed before core activation. WindowList snapshot adoption alone
does not complete menu/icon ownership. Actual slow-load WM input/timer checks,
the combined frozen-source Linux/BSD full suites, installed/default-X checks,
optional profiles and release sanitizer qualification remain pending.

The [ambiguous-image completion decision](../decisions/IMAGE-ASYNC-FORMAT-PROPOSAL.md)
also remains pending. The approved SVG-only decision does not authorize changing
compressed-XPM or other ambiguous-image completion timing.
