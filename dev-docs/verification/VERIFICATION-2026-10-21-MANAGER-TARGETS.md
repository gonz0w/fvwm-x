# Manager targets and feature handoff — 2026-10-21

Owners: SELECTION-01 #22 / DATA-01 #6. PR: [#148](https://github.com/gonz0w/fvwm-x/pull/148).
Status: **composed local checkpoint passes; PR #148 ready for maintainer review**.
[Machine receipt](VERIFICATION-2026-10-21-MANAGER-TARGETS.json).

## Outcome and milestone boundary

The approved [feature cutoff](../decisions/FEATURE-HANDOFF-2026-10-21.md)
ends general prerequisite work here. BUTTON-01, BUTTON-02 and CFG-01 follow,
then CFG-02/IPC-02. Twenty open general stage 5 owners are deferred to the
GitHub **Later** milestone. Their unfinished scope remains open; this is not
completion of all protocol, sandbox, text, packaging or release work. Bring
forward only a concrete feature prerequisite or reproduced serious defect.

This is one bounded admission repair plus independent dispositions for existing
conversions. The explicit prerequisite cutoff and independently reviewable
protocol contract justify this checkpoint; tests/docs are not counted as extra
behavior stories. No additional general audit is queued before button work.

## Contract and source review

ICCCM 2.0 §§2.2, 2.6.2 and 4.3 define owner request admission and manager target
conversion. Reviewed against the
[X.Org specification](https://xorg.freedesktop.org/archive/current/doc/xorg-docs/icccm/icccm.html).
The source changes preserve configuration syntax and existing dependencies.

- Direct TARGETS already returns the exact four supported atoms. TIMESTAMP is
  one INTEGER/32 acquisition value; VERSION is INTEGER/32 `[2, 0]`. Independent
  peers read type, format, length and values, including a fresh manager's
  acquisition. Legacy property-None fallback, unsupported-target refusal and
  MULTIPLE-None refusal remain usable.
- Mixed MULTIPLE conversions already produce typed results and process repeated
  destinations sequentially before SelectionNotify. Existing malformed/empty
  pair limits remain. A vanished requestor retains ordinary asynchronous X
  error handling; the next healthy request succeeds. These paths are retained.
- The old handler accepted synthetic requests naming another selection or owner,
  and requests preceding acquisition. One admission gate refuses those before
  conversion properties are read or changed. Both direct and MULTIPLE refusal
  preserve properties and correlated reply fields. A zero-initialized native
  XEvent holds the reply; this is explicit storage, not a demonstrated overread
  in the old FSendEvent wrapper.
- Pure policy covers CurrentTime, equality, before/after, wrap and half-serial
  boundaries. Review added a monotonic acquisition-age guard: after half a
  timestamp wrap (about 25 days), or with an unknown/backward clock, time alone
  does not refuse a potentially valid client. Foreign role/owner checks remain.
  There is no per-request X query, new X synchronization barrier or retained
  ownership history.

## Qualification and source boundaries

Host evidence is under
`builddir/codex-2026-10-21/manager-targets` in the owning workspace. Original and
review source archives remain immutable. Normal profile builds are reused;
configuration hashes, input identities, installed code/data and loader outputs
are captured. Unaffected broad evidence and affected review checks have separate
source identities. No full suite is repeated for PR creation or documentation.

The final C runtime is from the reviewed archive
`2e5393adb8c92c73dc1a91b90d70a05800633cac7a58b74d4766f447e3ab8ec3`;
the earlier broad candidate is
`02cffe40a4488295040e830149a0bac655972a90c04ebecae51635fc5ca41325`.
A later fixture-only cleanup observation changes manager_handoff_live.py;
production binaries remain identical during that continuation. Final upkeep
Markdown/receipt changes do not require another runtime qualification.

One combined broad checkpoint passes **2,396 standard entries and 38 selected
optional entries**, across Linux GCC/Clang on both architectures, Linux ARM64
software/minimal profiles and FreeBSD/OpenBSD 8.0 on both architectures. The
reviewed age guard has **72 affected continuation entries** across nine profiles;
OpenBSD ARM64's full standard run already uses that reviewed runtime.

The fixture-only continuation adds six Xvfb groups on Linux/FreeBSD and three
authenticated default-X groups on FreeBSD, with unchanged production binaries.
Four leak-enabled guest ASan/UBSan entries pass with actual `address,undefined`
configuration. The portable foundation passes 177 checks with 14 environment
skips (191 registered); it is separate from runtime qualification.

Ten final current-source installs verify **150 ELF, 77 compiled-data and 700
other data identities**. Final staged target conversions and manager handoff
run in each profile; geometry runs at 1x/1.5x. Nine authenticated default BSD
displays run in the composition, with 60 successful fixture groups and one
preserved original failure followed by the controls below. All nine servers
stop with status 0 and unauthenticated clients are refused. All three owned
OpenBSD QEMU jobs exit 0 and remove ephemeral keys; the ARM64 auxiliary wrapper
failure remains distinct from its passing runtime gates and composed loader
observation.

The PR144 performance reference remains at 3/5 subsequent merged implementation
checkpoints (PR145/146/147). Unmerged PR148 does not increment it; no new timing
comparison or tuning is due.

## Retained failures and dispositions

- The first fixture attempt failed before core launch because relative
  executable paths lost their leading `./`; supplied paths are now resolved.
  Subsequent old-core controls pass values/MULTIPLE, then independently record
  all three wrongly accepted admission cases and valid recovery. Corrected
  focused Linux ARM64 GCC / FreeBSD ARM64 policy and actual-X controls pass.
- Linux's combined run initially stopped at its 500 MiB disk preflight, before
  source adoption/build/tests. A finally observer then wrongly asserted candidate
  identity before adoption. Both failures are captured. Exact-hash retirement
  of 32 completed guest input-archive duplicates preserves canonical host inputs
  and reclaims 222,753,716 bytes. The first actual qualification resumes separately;
  no failed build/test is relabeled as passed.
- OpenBSD ARM64 passes its standard tests, fresh install, staged/default-X and
  geometry gates. An auxiliary staged-PNG observer subsequently uses a wrong
  directory name. The original wrapper remains failed with QEMU exit 0 and its
  ephemeral key removed. Composition uses the fresh staged-core loader log and
  the prior staged-worker loader observation: the worker's full bytes/mode and
  owned PNG payloads/environment are identical. No runtime suite or install is
  repeated to repair this observation. The final AMD64 observer derives the
  stage from the actual install summary.
- FreeBSD AMD64's affected Xvfb/install/staged checks pass, but its first final
  default-X run fails a combined immediate old-peer preservation / manager-window
  disappearance assertion after
  the expected process exit. One unchanged isolated default-X control passes all
  eight groups. The fixture now waits at most two seconds for the exact server
  resource/selection state: process exit does not order another connection's
  observation of disconnect cleanup. It still requires the old peer/successor
  to survive and the manager owner to disappear. Three Xvfb groups on Linux and
  FreeBSD and three authenticated native-default-X groups on FreeBSD pass with
  unchanged production binaries. The original failure and strict passing
  control remain; no broad suite is repeated for this fixture correction.

Completed guest capture/archive duplicates are retired only after canonical
host payload/identity validation, with exact node/hash/mode guards. Source,
reusable builds, install stages, performance references and VM disks remain.
Build-cache preparation count annotations are not used as passing test evidence;
acceptance counts come from source-bound test logs and explicit continuations.

## Remaining scope

Synthetic metadata refusal is role correctness, not an X11 authorization
sandbox. Server BadAlloc/property-write refusal has not been fault-injected or
repaired; XFlush does not prove server allocation succeeded. Future-time
validation, retained ownership histories, multi-screen replacement/race/flood
coverage and wider manager transfers remain with SELECTION-01. Script
PRIMARY/private roles, INCR, clipboard management and XDND are deferred. This
checkpoint does not establish full S06 or whole-role ICCCM conformance.

Native-CPU Linux sanitizer qualification, physical hardware, toolkits,
compositors and final release acceptance remain separate. No new configuration
command, dependency, rendering or package/install default is introduced.
