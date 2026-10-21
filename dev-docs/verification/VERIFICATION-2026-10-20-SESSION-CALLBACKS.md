# Swallowed-client session ownership — 2026-10-20

Status: composed local checkpoint passes; ready for PR review.
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

## Composed qualification

Three registration/pending-queue/property repairs pass a composed six-target/ten-profile checkpoint after one original full run: 2197 standard and 475 optional results. The composition retains 2516 unaffected original passes and uses 156 affected entries (122 fresh / 34 retained by exact inputs and eleven binaries per focused target). Fifty callback entries, twenty pending-peer entries, nineteen real-session drivers with 57 independent clients and 171 property replies, fifteen scoped guest sanitizer entries, nine fresh stages and four final authenticated default BSD session displays pass. Three inherited required OpenBSD AMD64 omissions and its install/two staged-geometry gap remain without retries. The existing no-automatic-save policy is preserved. Performance reference PR132 remains at 1/5; no comparison is due.

Final signed runtime `8f98be2c3661fc8982b07bad69a3061674b15665`, tree `fbd9d950c7f1ca4a9363cb278692d15bccd345e4`, base
`b6755cafe919df265a592d9759025b1e4e478398`; archive `1b8399d59ff27505ada583d1311fc5789cf55dd85e3893b6fb4de51a8610af2e` contains
2081 regular inputs. The original signed candidate and
source archive remain in the receipt with all six canonical captures. Final
runtime/test/build inputs differ from the original in exactly six paths: fsm.c,
the two callback/client probes, the constructor probe, session driver and Meson.
All other native inputs remain exact. Final upkeep changes guides/evidence only.

Five unchanged-main controls reproduce old-ID acceptance, NULL ID use, missing
property replies, duplicate pending retirement and a non-pending close while
another peer waits. All terminate at the corresponding assertions. The earlier
missing-initial-save control is explicitly withdrawn: the no-state proxy must
not create untracked checkpoint files. Refusal/retry and repeated get/set/delete
coverage pass on the final code. No exploitability inference is made from
injected allocation refusal.

The final 2672 passing results out of 2675
registrations retain 2061 standard and
455 optional original passes, replacing all
session-affected entries. The correction contributes 136
standard and 20 optional entries. Of these, 34 exact-input/
eleven-binary focused results are retained; 122 run fresh. Registration order
is preserved, with only two constructor/watch cases added per SM profile.
The minimal profile repeats its three affected policy/module controls.

All six final correction archives pass full gzip CRC, source/mode, payload,
runner and result validation. Every profile preserves its configuration and
core/MFL bytes. Unselected paths retain unchanged source and do not enter the
modified session callbacks; relevant core, Buttons and Script session fixtures
run again. No full matrix is repeated. The PR133 fortified-open fix remains
unchanged and passed the original full run.

Nine final fresh stages verify 135 code identities and 88 catalogs. Eighteen
code identities change from the original full candidate and 205 installed
identities remain exact. All non-Buttons/Script installed outputs remain exact;
all RPATH transformations and catalogs are checked. OpenBSD AMD64 retains its
three required CI/SVG/legacy-property omissions and no full install or two staged
geometry checks, without retries. The eighteen original geometry invocations
(126 cases) remain valid with unchanged core/MFL bytes.

The four original BSD default displays retain 62 unaffected commands; four
fresh scoped default displays replace only their session command with refusal
of unauthenticated X clients and clean server exit. Linux staged session checks
also repeat. In total nineteen final real-session drivers verify 57 independent
clients, automatic old-ID retry, 171 property replies and no automatic checkpoint
files. Eight default server starts occurred across original and scoped checks;
66 composed default commands qualify the final checkpoint.

Fifteen final guest Clang ASan/UBSan entries cover five callback scenarios and
nine constructor/watch scenarios with leak detection, plus real-session
integration with external-library leak detection disabled. This supersedes the
six earlier candidate sanitizer entries. Local emulated execution is separate
from native-CPU sanitizer acceptance. Fifty callback and twenty pending-peer
entry executions span the ordinary and instrumented profiles.

Portable verification ran once: 166 passes and
11 native-tool capability skips. The original full
26-page manual render is retained for 24 unchanged pages; two availability
clarifications were scoped-rendered, then the final two no-save descriptions
were scoped-rendered after review. Final inputs/outputs are bound explicitly;
no full manual rerender occurred. An isolated incorrect temporary parent path
failed before rendering and was corrected. Eleven original and eleven corrected
compile logs retain inherited diagnostics; no warning is on a changed C/header
line. No production hardening or test deadline changed.

Production C adds 52 lines and removes 27 (net 25). The private helper makes
pending-node ownership explicit without changing generic flist behavior. This
is not a measured speed or executable-size improvement. PR132 remains the
performance reference at 1/5 subsequent merged implementation checkpoints.

The user-authorized retention policy removes only exact-verified completed guest
capture duplicates after canonical host validation. Canonical evidence, source,
reusable builds, install stages and VM disks remain intact; [workflow](../WORKFLOW.md)
records the policy. [Machine-readable receipt](VERIFICATION-2026-10-20-SESSION-CALLBACKS-COMBINED.json). Physical GPU/input,
toolkit/compositor, persistent session-manager conformance and release acceptance
remain separate. Actions stay disabled; VESA #93 remains deferred.
