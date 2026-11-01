# File clipboard and drag-and-drop qualification

The file-only transfer component is qualified pending final delivery review.
FILE-INTEGRATION-01 #67 and whole SELECTION-01 #22 remain open. The [contract](../decisions/FILE-TRANSFER-2026-10-31.md) defines the
bounded browser/helper and cooperative Form work. Source32 closure is
`e503636feb62794c8f33f673248c59406ac19d1a5a9de1706088abf98a58cba5`;
archive SHA-256 is `b0034613317b97231b81b24c62490af7349b1f5d5e8d82331ba61f0af4dc2fc1`.
Production differs from source24 only in native helper C timer state; Go, other
C, headers and Meson are unchanged. Source32's UI fixture
(`a3f6700c3912f7e8cf619695341ac155ce204314b0c76237a093c0bf3144b7f0`)
uses an identical standalone stdlib module relay and bounded numeric RPC timing;
its five-second socket and ten-second ready deadlines and cases remain unchanged.
The raw 19-group fixture SHA-256 is
`2e05305c023d7043eb81cbf440bb36cd370e1a55dba646ba04f4a10204817c0b`.
The [receipt record](VERIFICATION-2026-10-31-FILE-TRANSFER.json) summarizes verified exports and actual nested JSON receipts, with paths and hashes
for detailed records retained in the host archives. Original
archives, including non-JSON output and screenshots, are retained under
`builddir/codex-2026-10-31/r1-file-transfer/evidence`.

## Observed results

| Evidence | Scope and outcome |
| --- | --- |
| Linux ARM64 gate27 | Normal and scoped native ASan/UBSan each pass 18 raw X groups and two C policy checks. Three themes each pass ten actual browser UI groups, with seven actual job Waits per profile and helper stdin EOF and Wait 0. The raw fixture uses independent clients and a stand-in Form peer; actual UI evidence is separate. |
| Linux ARM64 gate31 | Capture-only: one Copy group per profile across three profiles, nine PNGs total. Exact configurations and images were visually inspected and looked good. Teardown used intentional TERM, not EOF; this is not another ten-group run. |
| Linux ARM64 gates29 and 32 | Normal installed discovery passes nine groups with one named EOF skip. Minimal and software profiles each pass nine with the same one skip. Gate28's tagged install omitted the untagged Form manual; gate29's normal install corrected the fixture, without changing product build policy. |
| FreeBSD 15.1 ARM64 gate04 | Installed nine, raw 18, two C policies, three Go packages and two manuals pass. Gate07's authenticated 800×600 default server passes nine with one named EOF skip, seven job Waits and server Wait 0. |
| Linux AMD64 installed01 | Installed nine, raw 18 and default UI nine pass, plus two C policies, three Go packages and two manuals. This was emulated-CPU Debian 13, not Ubuntu. |
| OpenBSD 8.0 ARM64 installed01 | Source30 normal profile passes installed nine, raw 18 and authenticated default nine, each UI run with seven job Waits and one named EOF skip; two C policies, three Go packages and two manuals pass. |
| Linux ARM64 gate36, source32 | Helper-only normal and scoped native ASan/UBSan builds pass; raw 19 passes in both with leak checks. Installed UI passes nine with seven jobs and one named EOF skip. Go, policy, manual, theme and default sweeps were not repeated; retained identities compose. |
| FreeBSD AMD64 installed05, source32 | Controlled old-helper red reports `fresh completion feedback closed idle peer`. After helper-only build/install, raw 19 and installed/default authenticated UI nine each pass, each UI run with seven jobs and one named EOF skip. Prior source30 Go three, C two, manuals and four other actor identities are composed, not rerun. |
| OpenBSD 8.0 ARM64 and AMD64 installed02, source32 | Each helper-only dry plan contains exactly compile and link; installed nine, raw 19 and authenticated default nine pass. Each full UI run has seven actual jobs and one named EOF skip. AMD64 also passes one separate startup-only probe with zero groups/jobs. No deadline increase or exact Python-import root cause is established. |
| Linux ARM64 gate34 | Default dummy server passes nine with one named EOF skip and seven job Waits; authentication is unqualified. |

Installed helper diagnostics and exit status were unavailable: the installed
fixture observed browser Wait 0 and helper PID absence. The direct normal fixture
separately observed helper EOF and Wait 0. The source32 helper is qualified on Linux ARM64, FreeBSD AMD64 and both
OpenBSD targets. Linux AMD64 and FreeBSD ARM64 results above are prior
source24/29/30 base evidence, not source32 binaries. ARM64 Linux, FreeBSD and
OpenBSD executed native CPU instructions under
hardware virtualization; AMD64 runs used emulated CPUs.
These results do not qualify physical hardware, accessibility, or release.

Shared Form button coverage composes gate15's first seven cases with gate16's
four lifecycle cases on the same C binary; gate16 skipped the first seven.
Gate17's scoped fast-click Form ASan/UBSan pass is not full Xft leak coverage.
Gate01's two internal Go race packages pass on source02, whose relevant hash
matches current inputs. Its aggregate run failed on browser subprocess exit sleep;
gate24 browser normal, race and vet pass with `GORACE=atexit_sleep_ms=0`.

## Corrections and evidence limits

Original failures and diagnostics remain identifiable by source and archive in
the receipt record. They include early generation/owner-loss, malformed input,
Form gesture and fixture issues; two native P2 regressions and fast-drag red in
gate26 were followed by green gate27. The Go illegal drag-time regression was
red in gate23 and green in gate24. The earlier gate12 event schedule remains
unproven. Gate30 was a temporary capture diagnostic. The final two read-only
ListStatus turns are not a universal paint fence. Self-read cancellation was
covered; the regression does not fence the phase of an already-started self-INCR.

A later separate peer-agent review cleared source29 runtime with one P3 manual
phrase finding. The Shift/default Copy phrase was corrected and the exact
exported manual rendered with host mandoc exit 0 in gate35; final source31
documentation and evidence review remains pending. A separate source32 delta
review was clear with no new findings. Peer-agent review is not
final candidate acceptance or human/GitHub approval. Backend model metadata was
unverified. Separate final review remains required.

Gate22 generated both manuals; their exported roff hashes matched host macOS
mandoc reader runs, both exit 0. Gate21's Linux rendering attempt encountered
unavailable tools, not a content failure. Native BSD manual evidence is recorded
with the installed checkpoint. Gate22’s observed helper artifact size was 72,423 text, 1,824
data and 80 BSS bytes. One idle observation was 3,276 KiB RSS and 5,800 KiB
virtual size, not a comparison. Performance comparison counter is 2/5; no
comparison is due.

S16 file-only clipboard/XDND is adopted after acceptance, with cooperative Form
metadata and existing legacy cut buffers. S15 manager/SAVE_TARGETS remains
planned; general Script and core S13 are not qualified. Whole #22 includes
additional clipboard work and is not complete.

## Pending acceptance

FreeBSD AMD64 installed01 built the affected binaries and passed both C policy
checks and two internal Go packages. Its aggregate Go run failed the browser
`TestOwnedWorkerGroupRetiresOnEveryOutcome/flood` case: it returned cancelled
after 0.79 seconds under a 500 ms context. The harness reported no timeout or
remaining group members, but the fatal assertion skipped the heartbeat-stopped
check; the failed case does not prove its child stopped. Source analysis identifies
two Go startups and readiness, 4 MiB transfer, one write/read and kill/Wait
within that deadline, with context expiry taking priority. The exact expiry phase
and a product defect are not demonstrated. The lead accepted a five-second
non-cancel fixture watchdog while retaining the 500 ms cancellation case and
existing payload, status and heartbeat assertions. Gate35 passes all five
corrected outcomes in normal and race modes. FreeBSD
AMD64 installed02 passes those five, normal Go three, both C policies, build,
five installed artifacts and two manuals. Its UI passed only the first three
groups and three job Waits before a remote URI wait-label failure; raw and
default checks were unreached. The saved peer trace showed no selection request after Ctrl-V. Installed03
verified the source31 focus hit, selection change and commit, but repeated the
same group-four failure; the focus correction alone was not a demonstrated fix.
Previously passed build, Go, install and manual components were reused, and
raw/default were unreached. Separately native04 passed 18 raw groups with the
installed helper, without UI acceptance.

One private diagnostic reproduced group four with unchanged source/build/install
guards; it is diagnostic evidence, not acceptance. The analyst confirmed that
COMPLETE queues fresh feedback after a job exceeding five seconds while the
shared touched time is old. Deadline processing precedes POLLOUT and closes a
healthy peer. Subsequent genuine, focused Ctrl-V observes Form ready 0 and peer
-1. Malformed payload and the proposed Go early-attachment cause were not
established. Source32 separates input and output clocks: acceptance and positive
reads refresh input; initial queueing and positive writes refresh output. The
same five-second bounds remain, appends and opposite-direction traffic do not
renew the other timeout, and idle attached peers remain untimed. The controlled
red and source32 greens above qualify that correction. No production timeout
change or throughput within 500 ms is claimed. Linux and FreeBSD normal stops
are recorded in the receipt record.

OpenBSD AMD64 source30 installed01 passed same-profile refresh, four exact
predecessors, affected build, both C policies, normal Go three, normal install,
five binary identities and two manuals. Actual UI failed the initial ready/state
query with a `rpc_peer.record()` socket timeout before any group or job; raw and
default checks were unreached. Cleanup failures were empty and Xvfb, xrandr, core
and browser each had Wait 0. This initial-state timeout is not established as
the same cause as the later FreeBSD Paste silence. Both source32 OpenBSD continuations passed after helper-only compile/link.
Unchanged source30 Go three, C policies, manuals and four other actors compose
from exact source and artifact receipts; these were not fresh executions or
imports of whole historical passes. ARM64's compact export contains 176 members
and its full collection 4,335; AMD64's contain 201 and 4,608 respectively.
Both QEMU processes returned Wait 0; normal guest poweroff is not claimed.
Verified clones and input duplicates were retired (5,875,859,936 ARM64 and
5,150,798,245 AMD64 logical bytes); compiled payloads and current caches remain. The full collected payload of 4,415 members and
594,391,889
expanded bytes and artifact provenance were verified. Runner exit was 1 and
QEMU Wait 0; normal guest poweroff is not claimed. Old unmodified runtime
warnings remain retained. The ended owned clone and hash-verified input
duplicates were retired (5,112,336,125 logical bytes); current compact and full
compiled payloads, caches and source were preserved. OpenBSD ARM64 collection verification
covered all 4,303 collected members and the 179-member current manifest, with
the same five built-artifact provenance and no historical pass imports. QEMU
exited 0 through the unchanged runner SIGTERM/Wait; normal guest poweroff is
not claimed. Verified disposable clone/input duplicates were retired (5,855,027,000
logical bytes); caches and full payload were preserved. All OpenBSD labs
stopped. Linux was restarted only to prepare native bus/helper
policy and tag closure; no service fixture has run.

Whole #67 remains open for the FileManager1 installed-service criterion in
[FILE-MANAGER-PRIMITIVES](../decisions/FILE-MANAGER-PRIMITIVES-2026-10-06.md)
and retained PR315 verification. The transfer fixture explicitly excludes
FileManager1 and its common BINARIES omitted native `fvwm-session-bus`. The
lead selected a focused service closeout: eight enabled groups on six targets,
two disabled groups on representative Linux, FreeBSD and OpenBSD ARM, and one
Linux ARM service-to-file-transfer option observation, at most 55 observations.
Existing transfer, theme, default-X, sanitizer and detailed PR315 protocol
evidence is reused. The portable service fixture is being implemented; these
gates have not run. This does not add product features or complete whole #77.
Final delivery review remains pending; neither #67 nor whole #22 is closed and
no merge or whole acceptance is claimed.
