# M5 unused event-interface retirement

Status: focused checks pass; broader qualification pending. Owner: CLEAN-01
#34. Authorized PR243 is merged as
`403c341a5c71249861f1ebcc75752d14a389fe2d`; canonical signature/DCO and
reviewed tree `ae6915af9995f4a26edbad2274e5eda30d3e6b30` verified.
Its branch is retired and Project/labels Done. #35/#36 remain completed;
#93 remains superseded. This slice does not close the broader cleanup owner.

## Complete unused interface family

Remove six unused FEvent definitions and declarations: FWeedAndHandleIfEvents,
FGetMotionEvents, FAllowEvents, FCheckWindowEvent, FIfEvent and FPeekIfEvent.
Remove the orphan FSelectInput declaration, which has no implementation.
The two-file change deletes 115 lines with no additions. Exact reconstruction
from the recorded base removal spans preserves every retained source byte.

A tracked reference inventory finds only the definitions/declarations and one
historical expose-proposal reference; no native/test/config/installed consumer
calls these interfaces. libfvwm3 is a private static library, not an installed
header/plugin ABI. Keep the entire raw-X prohibition block, including its four
historical retired-name enforcement tokens. Those tokens are compile guards,
not callable aliases. Shared weed records/predicates/finalization and active
wrappers/cache/timestamp/queue bodies remain unchanged. No command syntax,
module protocol, dispatch behavior, dependency or install rule changes.

## Focused evidence

Capture FEvent with the actual normal GCC production compile command before
editing and after the change. Generated config.h is byte-identical. Remove only
the thirteen named complete declarations/definitions from the old preprocessing
stream. All 82,633 retained tokens match, apart from assertion diagnostic line
metadata: [618, 661, 681] becomes [618, 639, 659]. Each assertion's condition,
source filename and all other tokens remain identical.

The first private comparator expected two retained assertions rather than three.
Its failure and original script are retained. Inspect the actual source and
preprocessing, then correct only that exact diagnostic inventory. No production
edit, semantic-token normalization, expectation/deadline change, or repeat of the
successful preprocessing follows. Continue only the comparison and unrun gates.

One normal Linux ARM64 incremental group builds the core, all twelve C module
executables and four affected test/fixture executables. Existing resize-merge-
queue, expose-queue and fevent-hints units pass 3/3. The actual-core X expose
fixture passes 71 bounded collections, foreign-window FIFO, backlog/continuation,
rectangle merging, tear-off routing, nested cache and retirement controls.
The object symbol set loses exactly six definitions and gains none; all active
defined interfaces remain. FSelectInput is absent from both symbol sets.

Two changed inputs, 527 relevant C/header inputs and ten build/test inputs match
host/guest hashes and modes. Seventeen built output identities are retained.
Canonical private evidence is
`builddir/codex-2026-10-25/m5-library-retirement/native-evidence.tgz`:
46 regular payloads including the manifest, with all payload bytes/modes/hashes
and gzip EOF/CRC validated. The
[machine receipt](VERIFICATION-2026-10-25-M5-EVENT-INTERFACES.json) records exact
source/reference inventory, retained failure, commands, results and identities.
Repository checks/lint/links and working/staged whitespace complete delivery.

## Performance and coverage review

The due [five-checkpoint comparison](PERFORMANCE-2026-10-25-PR243.md) runs against
merged PR243 before source edits, using existing binaries. Browser bytes are
identical to PR238: median scans 8.793/8.778 ms with equal 25,664 KiB RSS and
overlapping samples. No implementation speed gain is established. Recorded
normal Script text/read-only is 13,571 bytes smaller, data 112 bytes smaller,
and its file 3,328 bytes smaller; no old-Script timing or WM latency/memory gain
is claimed. Verified PR243 core/Script/browser copies become reference 0/5;
this next implementation merge is 1/5. No benchmark rebuild or per-PR timing gate.

Accumulated risk is reviewed, not converted into an automatic all-OS run.
PR239's BSD saved-mask checks and PR240/241's active-body parity retain their
source scope; PR242/243 supply focused normal/scoped/X resource-owner evidence.
This deletion changes no active body or platform contract. No fresh sanitizer,
BSD/AMD64, install/default-server, full feature/theme or native-CPU/physical/
toolkit/compositor/release acceptance is claimed. Full milestone/release gates
remain pending with their owners. No new UI needs screenshots or a theme set.

The proposed environment/pathname family remains pending after a delegated
security-review block; no source edits or runtime reproductions were performed
for it. This ordinary unused-interface retirement is independent of that scope
and does not claim to resolve it. Broader #34 and other source/resource owners
remain open; no completed audit is reopened to count this cleanup.

Separate final source/prose/evidence review is clear. Verified raw host/guest
captures and reconstructed duplicate browser retire after archive/hash and
active-reference checks. Keep the canonical archive and actual PR243 reference
core/Script/browser copies, source/build caches and VM state. Signed author-DCO
delivery remains required. Requested
Astra/high analysis, Sol/high implementation and separate Sol/high review follow
project routing; runtime model metadata remains unverified.
