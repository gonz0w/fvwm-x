# Typed desktop settings checkpoint — CFG-01

Locally qualified first consumer checkpoint after merged PR150. CFG-01 remains
open for preview/recovery; this is not release acceptance.
[Machine-readable receipt](VERIFICATION-2026-10-22-DESKTOP-SETTINGS.json).

Three related behaviors ship together:

- One registry defines the seven existing preferences: IDs, types, defaults,
  choices/time bounds, descriptions, groups, owner and capabilities. Bounded
  per-setting query replies fit the existing core command limit.
- Validated changes, scoped reset and reload separate saved snapshots from
  helper-effective module overrides. Private versioned JSON commits before
  memory or theme commands; failed writes preserve prior state. Nonblocking
  writer locking, stale-snapshot refusal, no-follow regular-file checks,
  ownership/mode/link validation and an 8192-byte cap protect persistence.
- Native FVWM settings menus derive choices, saved/effective state and source
  from the registry. Explicit configuration stays authoritative; external
  personal Style/Colorset changes are reported as unobserved. No personal
  configuration is rewritten.

Preview is explicitly unavailable. Confirmation/cancel/timeout, generation
conflicts and consumer/restart recovery remain CFG-01 follow-up work. No new
setting domains, generic IPC result API, toolkit, core thread, dependency or
Go runtime port is introduced. Advisory locking serializes cooperating writers;
stale checks are not filesystem compare-and-swap against writers ignoring it.
Filesystem calls remain ordinary module-local I/O, not whole-call deadlines.

The consumer exposed two prerequisites. AddToMenu Greyed freed a caller-owned
label, leaked its replacement and dereferenced a missing replacement. The small
core repair keeps the original token borrowed and frees only the owned parsed
label after copying it. The helper had emitted Greyed as an action, making info
rows selectable, and ignored core-normalized colon-free configuration packets.
The shared menu producer and both packet forms are corrected and exercised.

## Source and qualification

Frozen runtime/test/build candidate:
9b4742fd8e7652ec2e3d1559fd791d7a351377f2; 2154 inputs; archive SHA-256
`d9ae763b491e4a86fe4c8d183f4bce1e618eeef881fc16418971d0d971ed474c`.
Publication changes only documentation/status/receipts afterwards. The final
manual list and snapshot wording have a separate source-bound render.

One combined six-target/ten-profile checkpoint composes 2420 standard entries
and 531 fresh selected optional entries (software/minimal), all passing. Of
2951 total entries, 2728 run fresh and 223 completed OpenBSD AMD64 entries are
retained from the same frozen candidate. Its 1200-second outer coordinator
budget expired after 223 completed console passes (215 had flushed structured
records). Exact binary, configuration, source and registration identities guard
retention; only the 73 unfinished registrations run afterwards, all passing.
Individual test deadlines stay unchanged. A subsequent artifact-only failure
assumed retained desktop captures were in the restored build cache. Their
original archive remains canonical; the final install/default-X completion
runs no registered tests again. Original failures remain represented below.

All ten fresh installs verify 150 native ELF targets, 77 compiled catalogs and
700 other files against built/generated inputs, including install RPATH changes.
The ten changed core executables contain the bounded Greyed ownership repair;
140 unrelated native targets retain PR150 identities. Every unrelated data file
and catalog stays identical; only ten generated FvwmDesktop consumers change.
Ten staged actual-X settings sessions and four authenticated default BSD-X
sessions pass; unauthenticated clients are refused. Disposable QEMU jobs shut
down with status zero and remove their ephemeral private keys.

Twenty policy cases run across the matrix, covering types/boundaries/unknown
IDs, legacy migration/version refusal, private file/parent/FIFO/symlink/hard-link
rules, stale/busy writers, fsync/replace failures and recovery, scoped reset,
persistence/reload and overrides. Actual-X cases cover all palettes, opaque bar
pixels, query/reset/reload, invalid/stale refusal, keyboard activation/cancel,
helper restart and explicit overrides. Each desktop fixture also performs 32
empty/quoted Greyed create/destroy sequences. Three leak-enabled guest
ASan/UBSan entries (desktop and two menu fixtures) pass with no diagnostics.
Portable verification records 191 entries: 177 passes and 14 environment skips.

## Evidence, corrections and limits

Canonical captures, input maps, complete payload hashes, installed vectors,
source/profile identities, review and cleanup receipts are linked in the JSON.
Successful owner-controlled guest capture/archive/source duplicates and the
positive sanitizer duplicate are retired only after canonical hash/mode checks.
Source trees, builds, stages, VM disks and failed diagnostics remain preserved.

Focused diagnostic archives/input fingerprints retain the oversized aggregate
query reply, menu/binding/window-attribute assumptions, reproduced core double
free and normalized configuration failure. The initial native/BSD preflight
receipt confused catalogs with executable counts; those attempts stop before
tests. The OpenBSD AMD64 timeout and artifact-copy failure retain their original
logs and both canonical archives; the composed disposition above supersedes
them without relabeling a failed coordinator as a passing full run.

The first manual table produced 24 unsupported table-spacing diagnostics.
Its list replacement renders correctly with Linux Asciidoctor and host mandoc.
Final lint remains exit 3: inherited insecure mso and unknown LINKSTYLE errors,
six old section-spacing warnings and one same-kind warning for the new section.
These formatter diagnostics remain DOCS-01-owned; no table diagnostic remains.

Native-CPU release sanitizers, physical hardware, toolkits/compositors and release
qualification remain separate. Guest sanitizer execution is not those results.
Performance reference PR149 remains at 1/5 after merged PR150; this unmerged
feature checkpoint triggers no benchmark or performance-only tuning.
