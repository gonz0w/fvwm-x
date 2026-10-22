# Manager targets and feature handoff — 2026-10-21

Owners: SELECTION-01 #22 / DATA-01 #6. Status: focused Linux ARM64 and
FreeBSD ARM64 checks pass; combined checkpoint pending.

## Bounded contract

The approved [feature cutoff](../decisions/FEATURE-HANDOFF-2026-10-21.md)
ends general prerequisite work here. BUTTON-01, BUTTON-02 and CFG-01 follow;
additional broad maintenance remains in the future-release backlog.

ICCCM 2.0 §§2.2, 2.6.2 and 4.3 define owner request admission and manager
TARGETS/TIMESTAMP/VERSION. Reviewed against the
[X.Org specification](https://xorg.freedesktop.org/archive/current/doc/xorg-docs/icccm/icccm.html).
This checkpoint does not claim complete ICCCM conformance.

- Direct TARGETS returns the existing exact four atoms; TIMESTAMP is one
  INTEGER/32 acquisition value; VERSION is INTEGER/32 [2, 0]. Legacy property
  None fallback and unsupported/MULTIPLE-None refusal are already implemented.
- Mixed MULTIPLE conversions already produce typed results and process repeated
  destinations in sequence before SelectionNotify. Existing malformed/empty
  pair bounds/refusal remain. A vanished requestor retains ordinary asynchronous
  X error handling; the next healthy request succeeds.
- The old handler accepted synthetic requests naming another selection or owner,
  and requests preceding acquisition. One admission gate refuses these before
  reading/writing conversion properties. Replies preserve correlation fields.
  The pure timestamp policy covers CurrentTime, equality, before/after, wrap
  and the half-serial boundary. X timestamps use 32-bit half-range ordering;
  no new per-request server query, synchronization barrier or history is added.
- A zero-initialized native XEvent holds the reply. This is explicit storage and
  initialization, not a claim of a demonstrated overread in FSendEvent.

## Focused controls and retained failures

Host evidence: `builddir/codex-2026-10-21/manager-targets` under the owning
workspace. The first fixture attempt failed before core launch because relative
executable paths lost their leading `./`; corrected by resolving the supplied
paths. Both subsequent old-core controls pass values and MULTIPLE. The final
baseline fixture records all three admission failures independently and valid
recovery. The repaired candidate passes property policy and all three real-X
cases on Linux ARM64 GCC and FreeBSD ARM64. Original logs/archives are retained.

## Remaining scope

Synthetic metadata refusal is role correctness, not an X11 authorization
sandbox. Server BadAlloc/property-write refusal has not been fault-injected or
repaired; an XFlush does not prove server allocation succeeded. Future-time
validation, more-than-half-range histories, multi-screen replacement/race/flood
coverage and broader ownership/transfer behavior remain with SELECTION-01.
Script PRIMARY/private roles, INCR, clipboard management and XDND are deferred.
No full S06 or whole-role conformance claim follows from the selected cases.

Native-CPU Linux sanitizer qualification, physical hardware, toolkits,
compositors and final release acceptance remain separate. No new configuration
syntax, command, dependency, rendering or package/install default is introduced.
