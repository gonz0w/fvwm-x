# Correlated MFL queries and read-only admission — IPC-02

The first IPC-02 subset is locally qualified. It adds three related features:
versioned bounded requests/errors; authoritative desktop/viewport/work-area
queries and processed-stream barriers; and one-way read-only admission.
[Machine-readable receipt](VERIFICATION-2026-10-22-IPC-REQUESTS.json).

The [request contract](../decisions/IPC-REQUESTS-2026-10-22.md) and FvwmMFL manual
define the exact flat schema, supported methods/resources and errors. A client
sends a newline-framed `request {"version":1,"id":"desk-1","method":"query",
"resource":"desktop"}` and receives a versioned terminal reply with the same ID.
Malformed/unsupported data receives explicit errors; unsafe or outstanding
duplicate IDs receive null-ID errors without terminating the older request.
Legacy text commands, subscriptions and JSON events retain their framing.
Supported FVWM configuration names, arguments and formats remain usable.

Desktop state comes from fixed `Send_Reply` expansions in the existing core,
with actual-pixel rectangles and desk/page indices. MFL owns only connection
mode, subscriptions and capabilities. The processed-stream barrier proves the
core reached that point in the same producer's command stream; it does not
prove applied command success, paint/persistence or asynchronous completion.
Local replies/events can overtake core-bound requests, so clients correlate IDs.
No second window-state authority, core API, packet type or Perl metadata changes.

IDs are bounded to 64 ASCII characters; the existing 1000-byte line limit,
32-client cap, callback/queue limits and write deadlines remain. Eight embedded
pending slots per client have absolute five-second monotonic deadlines. Fixed,
module-wide nonreused serials identify replies without retaining client pointers
in core work. Expiry/disconnect invalidates slots; late replies cannot complete
a replacement request or connection. Local queries work while the core waits.
Read-only mode permanently restricts subsequent commands/debug changes on that
connection, allowing queries/barriers/subscriptions. It does not undo queued
work, and reconnecting starts writable. This is accidental-authority reduction,
not an X11 security boundary or privilege broker. No network listener is added.

## Frozen source and combined checkpoint

Runtime/policy/observer candidate: `5f898d6e0abc05a009d6e429f84ab537be47afbb`;
2165 inputs; source archive SHA-256
`70cea803e822074f7a600e1be45282a14f0109b6ed541a4673843a9e4f735641`.
Base: merged PR153, `d330762f97ddb9cb9be5ab74cff0e57bdccd34af`.
Pre-freeze review corrected malformed duplicate-ID handling, absolute-deadline
checks when input precedes an already-due timer, and unattached JSON allocation
ownership. No runtime/test correction follows the freeze. Publication changes
only README/tracker/indices/evidence documentation; it does not repeat the matrix.

All 82 selected registered entries pass across six targets and ten profiles:
Linux AMD64/ARM64 GCC/Clang, Linux ARM64 software/minimal, FreeBSD AMD64/ARM64
and OpenBSD 8.0 AMD64/ARM64. Each profile runs `mfl-request`, `mfl-stream`,
`mfl-event-flags`, `json-serialization`, `mfl-request-xvfb`, `mfl-stream-xvfb`
and `desktop-baseline-xvfb`; six Linux profiles additionally run
`module-backpressure-xvfb` and `interaction-servicing-xvfb`. Commands use the
existing owner-controlled incremental builds, `meson compile -C BUILD -j 2`
and `meson test -C BUILD --num-processes 1 --print-errorlogs ...`.

The ten built request-X sessions compare core desktop/viewport/work-area state
with independent X properties, exercise command/query ordering, two clients
using the same ID, partial/coalesced input and callback continuation, strict
malformed/unsupported requests, one-way read-only refusal, timeout/ID reuse,
late replies/disconnected replacement and slow-reader/healthy-client isolation.
The fixture temporarily stops only its owned core to prove bounded pending
requests and local query progress, then always resumes it before teardown.
Pure native policy tests cover schema/ID/depth/NUL bounds, reservation and
serial isolation, fixed commands, checked core integers and JSON ownership.

Ten staged request-X sessions and four authenticated default BSD-X sessions
pass; unauthenticated clients are refused. All 927 installed file hashes/modes
are checked. Eight stages compose the new independent MFL file with additional
hard links to exact unchanged PR153 files; no prior inode is edited. Both
OpenBSD stages retain the original complete successful Meson install through
an exact hash/mode-verified cache. Their seven selected tests each remain
retained from the original frozen run; only the unfinished staged/default-X
checks execute in the completion jobs. Exact source, compiler/configuration,
test/binary, install-cache and package inventories bind this reuse. The 14
retained test entries are counted once in the 82 total.

Only ten native MFL executables change; the other 140 native ELF targets and
77 catalogs retain exact PR153 identities. Old broad MFL-dependent runtime
results remain historical and are **not** counted as current composed passes
for the changed MFL binary. The selected checkpoint verifies the changed
transport and existing affected consumers; it is not a fresh full release gate.
Canonical archives validate gzip EOF/CRC, source scope, complete payload hashes
and modes. Completed disposable QEMU jobs exit zero and retire private keys.

## Runner failures, sanitizers and remaining scope

Both original OpenBSD runs compile and pass all seven selected tests and the
complete Meson install. A private acceptance assertion then incorrectly demands
identical built/installed MFL bytes: Meson normally removes the build RUNPATH
and pads its former string. A bounded ELF64 verifier now requires the exact
RUNPATH/RPATH removal, otherwise identical ordered active dynamic entries and
otherwise identical bytes. It does not weaken product checks or change RPATH.
The first recovery attempt reads a retained-proof file from the wrong private
directory and stops before the remaining installed-session checks. The corrected
completion jobs reuse verified inputs; all original failed archives remain
canonical. No product change, unproven success relabeling or repeated matrix.

The first sanitizer setup looks up the new target before refreshing Meson's
registration and fails before testing. Explicit `meson setup --reconfigure`
registers the target; the unchanged candidate then passes the request policy
and actual-X MFL lifecycle with ASan/UBSan/LSan and leak detection enabled.
The MFL module is instrumented; its unchanged core is uninstrumented. No detector
is disabled and no suppression is added. The validated follow-up has no
sanitizer diagnostics. This is scoped emulated Linux ARM64 evidence, not
native-CPU/full-release sanitizer qualification. Existing PR152 Form
fontconfig/Xft process-exit allocations remain CLEAN-01 / Later and are not
requalified by this scoped request run.

Portable `make verify` passes 177 of 191 entries with 14 environment skips.
Final documentation/repository checks and local diff review pass. Linux
Asciidoctor renders the updated FvwmMFL manual; host mandoc formats it. Both
baseline/current lint exit 4: one mso and one LINKSTYLE warning remain; section
spacing warnings increase from seven to eight after SH and zero to four after
SS for the added section/subsections. This is not clean manual lint; DOCS-01
retains formatter polish. No new UI is selected by this transport-only batch;
existing desktop-X captures remain canonical, and later visible consumers
retain actual-X screenshot review.

General topology/binding/registered-settings providers, typed domain actions
with applied/failed results, progress/cancellation, service lifecycle and
observable shared-core generations/CFG-01 preview remain open. IPC-02 stays a
partial owner in review. General maintenance, visual polish and tuning remain
deferred. PR149 performance cadence is 4/5 merged checkpoints; no comparison
is due until the next implementation merge. Native-CPU sanitizers, physical
hardware, toolkit/compositor and full release acceptance remain separate.
Owner-controlled guest capture/archive duplicates are retired only after
complete canonical verification; sources, builds, stages, failed/success host
evidence and VM disks are preserved.
