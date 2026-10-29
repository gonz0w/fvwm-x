# Startup group event ownership — 2026-10-29

This checkpoint extends #56's startup observation to late IDs on referenced
unmanaged ICCCM group leaders. It batches dependent identities, cooperative event
masks, managed/unmanaged transitions, lifetime retirement and bounded outer work.
It does not close the whole installed-platform interoperability issue.

[Exact source, commands and results](VERIFICATION-2026-10-29-STARTUP-GROUPS.json)
· [Selected contract](../decisions/STARTUP-NOTIFICATION-2026-10-28.md#group-observer-completion-selected-2026-10-29)
· [Tracker](../PROJECT-TRACKER.md#r1-startup-group-event-ownership--2026-10-29).

## Scope and independent expectations

No `FvwmWindow *` survives a turn. Fixed tables retain up to 256 raw client XIDs
with current palette identities and 256 reference-owned leader records. Current
base masks, supplemental interests and nested temporary suppressions are separate.
Unmanaged leaders keep StructureNotify while referenced; PropertyChange exists
only while startup activity is pending. Managed leaders preserve core suppression
and lifecycle handling. Admission refusal increments bounded diagnostics without
eviction or preventing application launch.

At most eight X-dependent group/client work steps run at the safe outer boundary.
New pending launches revisit existing clients. Nested service/deadline paths stay
pure. Capacity and cooperative ownership are tested as finite contracts, rather
than inferred from one visible launch.

The independent Xlib producer neither links nor calls the startup observer. The
first baseline attempt only waited for reparenting and passed through an initial
map race; it was rejected as an oracle. The corrected baseline first witnesses a
unique class observation and actual M_MAP, then starts a new ID without a class
and changes only the unmanaged leader. The old merged-PR299 core remains pending
and fails the observation deadline. Producer/module/core/Xvfb cleanup returns zero.
This establishes the late-ID regression before implementing its correction.

## Focused results

Final runtime closure `8614edc47eebe49d05bbc0bcf650c7552986b74fd8fe1bc8803c6cd89b607431`
covers 578 guarded inputs. The source-bound receipt records commands, binary hashes,
case results and cleanup. Affected normal GCC core/policy builds and strict Xlib
producer compilation pass in the owned native Linux ARM64 lab. The two portable
policies pass; the new ownership policy also passes ASan/UBSan with leak checking.

The 15-case normal groups run passes once with the real 60-second deadline:
late IDs after original map, already managed bindings/new pending work, shared
references, hint replacement/clear, direct/malformed-direct precedence, leader
destroy and explicit rebind, idle reparent/destroy, managed iconify/deiconify/desk/
live withdrawal, actual foreign reparent, initial iconic capture, override-redirect
base changes, more than one outer budget, synthetic destroy refusal and foreign
icon aliases. The separate monitor observes removal of the PropertyChange claim
at 60.037 seconds while unrelated native events continue, before any core expiry
query; StructureNotify remains until the last dependent retires, then mask zero.

The older protocol phase passes 13 executed groups, with its real-expiry row
explicitly skipped because the group phase already owns that one interval.
The changed core passes a scoped ASan/UBSan groups run with leak checking,
unchanged normal modules/helpers and no second real-expiry wait. Actual process
and module waits and empty cleanup-failure arrays are part of the receipts.
This is native Linux execution, not emulated/native-CPU substitution.

Fresh hardware-accelerated FreeBSD 15.1 ARM64 and OpenBSD 8.0 ARM64 guests each
compile and execute the two portable policies with assertions and strict warnings.
All six input file hashes and both evidence archives verify, and both QEMU exits
are zero. OpenBSD's run preserves a compiler `strcpy` warning. These are native
policy checks only: no new BSD core, installed WM or BSD X acceptance is claimed.
Both disposable working disks retire after archive/ownership/live-use checks;
verified compressed seeds and compact evidence remain.

The final queued-lifetime correction adds a focused unit covering empty/complete
and 64/65/128-event prefixes, actual/stale/synthetic/wrong-window distinctions,
match priority over incomplete, later ordinary dispatch, appended tails, exact
FIFO bytes and current/previous event-cache/timestamp preservation. Existing
expose/resize queue and FEvent hint units pass. The final normal group and
protocol phases and scoped core/queue ASan/UBSan checks pass after this correction;
the earlier real-expiry result is retained with its exact original source. The helper uses only the
already queued snapshot. Registry and scope state are 49,180 bytes; 64 XEvent
scratch entries add 12,288 bytes on this target, totaling 61,468 bytes under the
64 KiB assertion. Including the 216,104-byte startup activity state remains below
512 KiB. These are bounded state/scratch sizes, not measured resident memory.

The affected FvwmDesktop and FvwmMFL man pages generate with Asciidoctor.
Repository foundation and working/staged whitespace checks apply at handoff.
No Go, UI, toolkit, theme or package contract changed; reuse their earlier scoped
results without a new all-OS/theme/install sweep or new screenshots.

## Corrections and retained failures

The first candidate exposed an existing capture assumption: a watched leader's
new StructureNotify subscription delivers FVWM's own direct reparent event. The
handler accepted the frame but rejected the actual parent window, withdrawing
the client before M_MAP. The correction accepts the current parent only for the
actual client window. A separate real foreign-reparent case proves genuine
withdrawal still removes only the leader from the actual managed-client list,
retains the dependent's subscription and releases it at last reference.

The old protocol fixture reused a successful dependent's live leader for later
malformed-ID cases. Correct new late observation completed the later ID through
that earlier client. Each independent case now owns its own leader, preserving
all original pending/exact-window assertions. The group-phase source is identical
across this fixture-only correction, so its passing real-expiry evidence is
retained. Lead setup failures (a missing client variable and new-file manifest handling)
are preserved separately from product evidence. The first final-guard build
also rejected a raw XSendEvent call after the FEvent header became visible;
using the existing passthrough FSendEvent wrapper fixes that compile failure.
No runtime expectation or compiler policy was relaxed.

Separate review also identified two lifetime boundaries. Initial attribute import
and subscription now use a balanced existing nested server grab; its existing
synchronization is intentional and no extra barrier is added. Inherited-property
X errors abandon bindings without restoring a mask onto a possibly dead XID.
After a property reply, a bounded queued-only destroy check prevents accepting a
replacement ID before the already queued actual destroy is dispatched. Incomplete
queue prefixes defer the observation and stop the outer batch. The queue helper
preserves FIFO and cached input state, without socket reads or recursive dispatch.
Actual same-XID replacement timing was assessed from source/protocol ordering;
no deterministic native reproduction of that particular timing is claimed.

## Remaining qualification

Known internal XIDs are rejected using existing contexts. Creation-only internal
windows without those contexts have a non-overwrite source audit, not a new
registry or dedicated rejection fixture. Legacy tray transition coverage and
same-XID native stress are not inferred from the group fixtures. Stale serial,
capacity and resource reuse rules retain their pure-policy evidence.

#56 remains open for the rest of its frozen installed-target acceptance. #67
retains optional FileManager1 under #77 and broader lifecycle/target evidence.
Full compiler/feature/default-X/install/six-target release gates remain at their
accepted boundary. Pre-acquisition stale raw WM_HINTS XIDs are historically
ambiguous in X11; post-acquisition lifetime is the observer's contract. No
focus/placement policy, resident, dependency, bus stack or configuration migration
is introduced. #257/#258 remain parked.

The [due performance observation](PERFORMANCE-2026-10-29-PROVIDERS.md) used cached
merged-PR299 artifacts before these changes. It is not a benchmark of this core.
The next reference is current at 0/5, advancing to 1/5 only on the next runtime
merge. Keep only the current reusable builds, current review/failure evidence and
next reference under the retention rule; no new full duplicate build archive.

## Evidence and cleanup

The 462,794-byte host archive contains 242 regular payload files (8,976,067
uncompressed bytes) plus the 52,247-byte embedded manifest: 243 members and
9,028,314 uncompressed bytes in total. SHA256
`ea8fc7fb7a61a6a2fb8a5022eba5d6ed35663bd99581a79872f58fc2e78b0320`.
Every payload hash/mode, archive name/type and gzip CRC verifies. Retire 240 guest
duplicates only after that verification, retaining current source/result and a
small continuation receipt. Current normal/sanitizer builds, source, producer,
stage, SDK/tool caches and next performance reference remain. The two completed
BSD working disks retire after evidence verification, recovering 6,307,889,152
allocated bytes. Both QEMU jobs exit zero and the owned Linux container/VM stop
normally. No disk expansion or blanket shared-store pruning occurs.

Implementation, fixtures and separate source review use the selected Temper
routes; the lead owns native execution, Git/GitHub and evidence reconciliation.
No role was unavailable; actual backend model/effort metadata remains unverified.
The independent agent review is not a human approving review or release approval.
