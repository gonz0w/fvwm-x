# Swallowed-client session replies — 2026-10-20

Status: implemented; focused Linux/FreeBSD checks pass; combined qualification pending.
Owners: DATA-01 #6, FD-01 #7 and CLEAN-01 #34. Base: merged PR #133.

Three related behaviors in `libs/fsm.c`:

1. Reject IDs from unrestored sessions, freeing callback-owned input so libSM
   retries fresh. Check generated IDs before replying and retain cleanup/retry
   after reply allocation refusal.
2. Send the initial local, non-shutdown, non-interactive save after successful
   registration. Existing phase-two and save-complete callbacks finish the
   exchange. Applications may write their own checkpoint files.
3. Complete every property request with an empty reply under the existing
   swallow-proxy discard policy, including after set/delete requests.

The [X Session Management Library v1.0 server callbacks and registration contract](https://xorg.freedesktop.org/archive/current/doc/libSM/SMlib.html)
provides the operation contracts. The existing proxy still discards properties
and restores no sessions; this bounded repair is not complete XSMP session-manager
conformance or adoption of the later SESSION-01 desktop service.

Five actual-library callback scenarios exercise old-ID rejection, generation
refusal, reply refusal, initial save/phase two/completion and repeated property
replies, with owned-string release and cleanup. Independent libSM clients use
real ICE authentication, automatically retry an old ID, finish three save
exchanges and receive nine property replies per driver. Existing core, Buttons
and Script integrations remain in scope. No command/configuration migration,
packet changes, new dependencies or new main-loop barriers.

The first focused integration correctly exposed a fixture assumption that the
save directory contained authentication files only. The new initial save makes
FVWM write its own `.fs-` checkpoint. The corrected fixture separately checks
that checkpoint's FVWM version marker and absence of `.fsm-` authentication files.
A mistaken test dependency name and void-return assertion were corrected before
acceptance; all failed preparation/focused evidence is retained. No production
hardening or deadlines were changed.

Performance reference PR132 is accepted. PR133 is the first subsequent merged
implementation checkpoint (1/5); no regular performance comparison is due for
this batch. No measured speedup or size reduction is claimed.
