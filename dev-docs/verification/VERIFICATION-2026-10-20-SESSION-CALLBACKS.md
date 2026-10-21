# Swallowed-client session ownership — 2026-10-20

Status: implemented; focused correction passes; composed qualification pending.
Owners: DATA-01 #6, FD-01 #7 and CLEAN-01 #34. Base: merged PR #133.

Three related behaviors in `libs/fsm.c`:

1. Reject IDs from unrestored sessions, freeing callback-owned input so libSM
   retries fresh. Check generated IDs before replying and retain cleanup/retry
   after reply allocation refusal.
2. Retire only the matching pending ICE node, once. A close watch and status
   sweep may both retire the same connection; a watch may close an already
   accepted connection. Absence must preserve all other pending peers. The
   generic flist removal returns NULL on absence, which previously lost the
   remaining pending queue. The operation-local helper owns one list node and
   unlinks it in one walk; generic list semantics are unchanged.
3. Complete every property request with an empty reply under the existing
   swallow-proxy discard policy, including after set/delete requests.

The [SMlib v1.0 server callback contracts](https://xorg.freedesktop.org/archive/current/doc/libSM/SMlib.html)
identify registration input ownership and reply behavior. The
[ICElib v1.0 connection watch contract](https://xorg.freedesktop.org/archive/current/doc/libICE/ICElib.html)
permits close watches during connection retirement. The existing proxy still
discards properties and restores no sessions; this is not complete session-manager
conformance or adoption of the later SESSION-01 desktop service.

The final proxy preserves its existing no-automatic-save policy. An initial-save
proposal was withdrawn during final review because the discard-only proxy cannot
manage the resulting client checkpoint files/discard commands. The original full
candidate and its evidence remain recorded. The final candidate replaces that
proposal with the reproduced pending-queue repair, and repeats only affected
session checks. It does not repeat the full matrix.

Five actual-library callback scenarios cover old-ID rejection, ID/reply refusal,
retry, the no-save policy and repeated property replies. Two additional
constructor/pipe/watch scenarios verify a rejected connection's recursive close
and the close of a connection no longer pending, each while another peer remains
pending. Both peers eventually close with no retained watch or descriptor.
Independent libSM clients verify real authentication, automatic old-ID retry,
nine property replies per driver and no automatic checkpoint files. Existing
core, Buttons and Script integrations remain in scope.

No supported command/configuration migration, packet changes, new dependencies
or new main-loop barriers. Production/window authority remains single-threaded.

Failed preparation attempts are retained: an undefined test dependency, a void
API assertion, and earlier fixture assumptions about checkpoint files were
corrected. The old-main missing-initial-save observation is withdrawn with the
proposal; it is not a defect claim. Registration/NULL-ID/property controls remain
valid, and separate pending-close controls reproduce loss of the other peer.
No production hardening or test deadline was weakened.

Performance reference PR132 is accepted. PR133 is the first subsequent merged
implementation checkpoint (1/5); no regular comparison is due. No measured
speedup or executable-size reduction is claimed.
