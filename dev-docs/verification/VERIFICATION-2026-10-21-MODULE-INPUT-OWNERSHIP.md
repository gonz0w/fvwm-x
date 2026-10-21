# Module input and inherited-resource ownership

2026-10-21 — DATA-01 / FD-01 / PROC-01 / CLEAN-01, with bounded-reader
LOOP-01 coverage. PR #142. Combined qualification is running; this report's
final outcome/counts remain pending. Broad audit owners and release acceptance
remain open.

## Four related repairs

1. **Bind logging metadata to actual inheritance.** A module's configuration
   reply now reports its fork-time descriptor, stored as a borrowed number in
   the core's module record. A later core log toggle cannot transfer an fd into
   an existing process. Native Buttons, Backer, Ident and IconMan consumers use
   once-only inherited adoption, including disabled/failed first adoption.
   Repeated metadata cannot re-adopt a closed descriptor now used by an unrelated
   child resource. The generic transactional `log_set_fd` API and explicit
   `log_open` recovery remain available. No extra descriptor copy or per-child
   open file is added. SIGUSR2 only sets a deferred flag; main-thread logging
   changes cannot invalidate the snapshot between its assignment and fork.
2. **Terminate damaged packet framing.** The legacy reader refuses a bad start
   word immediately instead of scanning an unlimited word stream or treating a
   body word as another header. Invalid counts and truncated EOF report EPROTO;
   clean EOF reports zero errno. NULL ends that API's stream contract. The
   incremental reader makes bad framing/truncation sticky, distinguishes clean
   EOF and preserves partial/EAGAIN/EINTR continuations, packet order, one-packet
   publication and its 64-read service bound. Existing blocking startup reads
   remain blocking; no main-loop blocking operation or new timeout is introduced.
3. **Retain Script message token ownership.** `SendMsgAndString` uses the checked
   integer parser's borrowed continuation after the two arguments. The former
   two disposable `GetNextToken` calls lost the first allocation on every valid
   dispatch. Quoted numeric arguments, SendString/CheckBinding and exact
   LastString payload bytes remain usable. No new parser/global or API rename
   is needed.

4. **Reject requests without a module recipient.** Source review reproduced
   a pre-existing core SIGSEGV when root configuration calls Send_ConfigInfo.
   Check its missing module context before calling any module-only sender,
   report the refusal through existing diagnostics and return safely. The live
   fixture exercises this request before module startup. This late confirmed
   crash gets an affected continuation after the original matrix, without
   changing running inputs or repeating full suites.

Configuration names/arguments and native packet layout remain. The main manual
documents module logging's launch-time selection: restart a module to attach
it to current core logging. This is an internal ownership/failure contract
repair, with no configuration migration or new desktop dependency.

## Source and focused evidence

Base: merged PR141 `d65edcb24323849051a0e43ec267235ae3ef2915`.
Frozen signed author-DCO implementation:
`3627a389b85f07a3228d80832ed83ddf22f5edfa`.
Its 2,111-input source archive SHA-256 is
`7669a03eaeff4cb0fb229cdee0faa854588698477854d36b1aec2fcc91feeee6`.
Native source/build inputs stay frozen during the matrix.

Eight focused checks each pass Linux and FreeBSD ARM64 before qualification:
legacy packet read, incremental packet reader, configuration-line views,
Script message ownership, log-stream ownership, actual delayed-module logging,
Script packet admission and Script values. The later fixture diagnostic guard,
header comments and test wrapping are included in the frozen matrix candidate.

The real logging fixture launches a bounded delayed module twice: no log at
fork followed by enabling the core log, then an inherited log followed by
disabling it in the core. Two configuration requests per child must keep the
actual inherited number. Only the known inherited sink is written; private
child resources stay unchanged. Barriers and deadlines bound the probe, and it
supports a supplied default BSD display as well as nested X.

The linked-production Script routine probe performs 400 dispatches with quoted
integers, empty payload and literal punctuation/space cases. Wrapped token
allocation/free observations require no outstanding token owners after each
call. It does not claim to map a widget; the existing Script GUI/transport
fixtures cover the live consumer separately. Log-stream controls fork three
isolated cases: initially disabled, failed sink with number reuse and failed
adoption, each with 100 repeated metadata deliveries. They assert descriptor
and data ownership explicitly; forked children use `_exit` rather than running
a separate child leak scan.

Framing units cover every truncated byte position, invalid counts, bad start
before a valid-looking following header, terminal incremental errors, real
pipes, interrupted/fragmented valid packets and clean EOF. The service-budget
test now injects EINTR rather than relying on discarded junk words and retains
completion on the final permitted read without another readiness wakeup.

## Expected-negative controls and retained setup failures

The preceding production Script objects fail the new ownership observation at
its first dispatch: the first skip token is orphaned. This is an allocator
ownership observation, not a claim of a prior sanitizer finding.

A separate isolated startup control using the same preserved core runs
`Send_ConfigInfo` outside a module context and exits with SIGSEGV (status -11).
It confirms the fourth repair's entry-point failure; no host desktop is used.

The preserved PR138 core, SHA-256
`9993749c51adc1c494d03556a8ea0d5ed6ea5ab3fc337f4c05c697425da1b375`,
advertises descriptor 4 twice to a delayed module whose inherited log is -1.
The relevant fork/configuration descriptor provenance is unchanged through
PR141. The corrected control records that mismatch without writing through an
unknown descriptor. It does not demonstrate an exploit or claim an arbitrary
write. The new log-adoption unit separately verifies refusal to reacquire a
number reused for unrelated owned storage after sink failure.

Original baseline setup failures remain: the first driver lacked its helper
import path; a second lacked the helper path in the exec'd child; a third tried
to write the incorrect descriptor and received EBADF before publishing its
result. Correct only the isolated control, retain those logs, and do not count
them as defect reproductions or rerun passing native suites for them.

## Selected input-review dispositions

| Selected entry point | Disposition at this source |
| --- | --- |
| `ReadFvwmPacket` and shared incremental framing | Damaged-stream failure/EOF contract repaired; valid continuation/order and bounds retained and exercised |
| `GetConfigLine` | Already bounded: initialize output, admit declared text beyond three context words, require in-body NUL, trim unsigned-byte whitespace and bound request construction; its function body and configuration-view driver match merged PR141 exactly |
| `script_packet_is_valid` and immediate Swallow consumers | Existing declared-word/text checks match body consumers: M_MAP uses body[0], names use text at word three, property bodies require their fields/text; helper/driver/live transport inputs match merged PR141 |
| `script_value` helpers | Existing checked integer/tag/arithmetic, hex width, substring and record framing remain; source/header/driver match merged PR141, including malformed/truncated tail rejection; no rewrite |
| Immediate Script incoming-message consumer | Repeated-message token ownership repaired using the existing checked parser |
| Root Send_ConfigInfo without a module context | Confirmed old-core crash; early recipient check repaired, affected qualification pending |
| Delayed/repeated LoggingFD producer and native consumers | Fork-time identity and once-only adoption repaired; the source concern from PR141 has a concrete old-binary mismatch and bounded new controls |

These dispositions cover the selected input family, not every module, parser,
property, decoder or subprocess in the project. Pending interaction lifetimes
remain the next finite review family. Read-only inspection of existing deferred
window destruction and command contexts is preparation, not new acceptance.

## Combined checkpoint

Pending final six-target receipts. Run the full standard matrix once, selected
affected optional profiles and scoped ASan/UBSan with leak detection, then
native install/staged/default-X and manual checks. Retain original failures and
use affected continuations only when needed. Final comments/line wrapping in
three C/test files preserve all C tokens; test assertion failure line numbers
may differ. Frozen native inputs are untouched.

Portable `make verify` already completes with raw exit zero: 188 entries,
177 passes and eleven capability skips in seven groups. Creating/updating the
PR does not trigger another portable suite or matrix.

Canonical evidence belongs to
`builddir/codex-2026-10-21/module-input-ownership/`. After complete capture and
identity validation, retire exact owner-controlled guest output/archive
duplicates. Preserve canonical host evidence, source, reusable builds, stages,
performance references, disks and other owners' work.

PR138 remains the performance reference at 2/5 subsequent merged implementation
checkpoints after PR139/141. This unmerged PR does not increment the count and
no comparison is due. Pure tuning stays deferred. Guest-native binaries and
scoped sanitizer runs do not establish native-CPU sanitizer, physical
GPU/input/hotplug, toolkit/compositor or final release acceptance. OpenBSD
follows new releases; this checkpoint targets 8.0 and excludes 7.9.
