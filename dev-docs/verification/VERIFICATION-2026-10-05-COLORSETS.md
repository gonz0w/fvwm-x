# Sparse colorset storage — 2026-10-05

Status: completed. The user approved the 4096 per-process distinct-set limit,
build-time override up to 65536, and consistently copying set zero when a
bundled module first references an undefined ID. All seven final local build,
test and fresh-install gates and the four default BSD X-server gates pass.

## Implementation

The core and bundled native modules share a checked sparse store. A sorted
index provides binary-search lookup; separately allocated records keep stable
addresses across bounded index insertion/growth. Consecutive rendering field
reads use one exact-hit cache. Rendering lookup does not allocate. Creating
Colorset 100000 or INT_MAX creates that record rather than every preceding ID.
No runtime library is added; authoritative X state remains single-threaded.

The core initializes its existing black/gray and ID-dependent monochrome
record defaults. Modules initially copy zero for an undefined referenced ID.
An unrelated higher core ID no longer materializes implicit gray gaps. Themes
that relied on a gap should define the desired colorset explicitly. The copy occurs when the record is first materialized; the new record does
not automatically track subsequent changes to zero.

Only allocated IDs are broadcast at startup, in ascending order with zero
first. Root transparency and button redraw iteration use actual IDs. Numeric
IDs and packet fields remain unchanged; the Perl colorset tracker uses a hash.
Checked conversion rejects overflowing IDs before allocation. Migrated style,
menu, decoration, expansion and module assignment paths check admission before
replacing assignments or freeing their prior resources. Existing sets remain
editable at capacity. Multi-ID admission can retain newly created records if
a later requested ID is refused; prior assignments remain unchanged.

Supported compilers now reject implicit function declarations and incompatible
pointer types. This catches missing declarations rather than allowing pointer
truncation on LP64; it is not a blanket warnings-as-errors setting.

## Final verification

| Target | Full tests | Colorset X outcomes | Default BSD X colorset outcomes | Fresh install |
| --- | --- | --- | --- | --- |
| Linux ARM64 GCC | 73/73 | 7/7 | not applicable | passed |
| Linux ARM64 Clang/lld | 73/73 | 7/7 | not applicable | passed |
| Linux AMD64 GCC | 73/73 | 7/7 | not applicable | passed |
| FreeBSD ARM64 | 62/62 | 7/7 | 7/7, default Xorg | passed |
| OpenBSD ARM64 | 62/62 | 7/7 | 7/7, Xenocara/wsfb | passed |
| FreeBSD AMD64 | 62/62 | 7/7 | 7/7, default Xorg | passed |
| OpenBSD AMD64 | 62/62 | 7/7 | 7/7, default Xenocara | passed |

Sixteen widget, six storage, seven execution and 24 selection/Form outcomes also
pass under nested X and all four default BSD servers. All seven collected
archives match 156 frozen fingerprints and 1155 source permission entries.
Fresh installed core/module code and constants match their tested binaries;
full build hashes match the corresponding installation records. These are local
Podman Linux and isolated QEMU BSD gates on macOS, including emulated AMD64.
The four snapshot guests have shut down and their processes have exited;
unrelated desktop guests and the Linux verification containers are preserved.

## Coverage

Native store/API coverage checks sparse INT_MAX/100000 IDs, ordered iteration,
reverse growth, stable pointers, refusal without existing-record/count loss,
updates at capacity, initialized zero/cloning, packet round trips and checked
int32 conversion. Pure storage policy passed Linux GCC/Clang and all four BSD
architecture targets. Separate Linux ARM64 builds exercised actual configured
limits 1 and 65536 through policy/API tests; configuration rejects 0 and 65537.
The high-ID X fixture skips below four sets, with that skip path checked for
1, 2 and 3. Other policy/API tests remain enabled at those settings.

Seven actual-X outcomes check allocated-ID startup packets without gaps,
undefined module IDs copying red zero despite a higher core ID, live module
updates, full core/module tables, count/overflow refusal without new packets,
preserved visible styling/assignments, existing updates and frame repainting at
capacity, diagnostics and continued WM availability. The full ID lists and
Script color reports are retained.

The table count bounds records, not whole-process memory, images, gradients,
X-server resources or IPC queues. The 65536 build setting is covered at the
storage/API layer; no full 65536-set X/module startup claim is made. Existing
module transport budgets still apply. Monochrome defaults are preserved in
source but this pass does not add a monochrome X-server integration check.
Broader borrowed-resource lifetime and modal/descriptor servicing work remains.
Native Linux ASan/UBSan CI is configured but unrun in this checkout.

## Corrections and provenance

The historical [candidate report](VERIFICATION-2026-10-05-COLORSET-CANDIDATE.md)
records compilation/fixture/source-isolation corrections and partial checks.
A separate previous-installed-versus-sparse actual-X control reports implicit
gray versus copied red zero. Its helper checks unchanged before/after binary
hashes; those sparse binary hashes exactly match the final Linux ARM64 GCC
core and Script. The control's initial invalid literal report filename was
corrected; failure metadata is retained, while the original helper log was
overwritten as disclosed in the candidate report.

The first full FreeBSD ARM64 selection run checked the child window immediately
after its asynchronous launch acknowledgement and failed that observer. It
now waits on a bounded window predicate. Initial failed logs/archive are
preserved. Only this observer changed between initial and final full runs;
production source and binaries did not change. All final gates use its corrected
fixture, rather than treating a rerun of the old observer as a fix.

The final overlay inherits the previous verified source and adds this pass's
owned changes. Concurrent desktop/launcher/session work is excluded. The tests
build definition combines the previous verified baseline with colorset targets;
projection hashes and the shared-checkout differences are recorded. The current
checkout differs from the verified fingerprints only in that intentional test
projection and the other chat's desktop fixture. Final installation verification
uses a fresh empty stage and compares core/module .text and .rodata, with build
and installed runtime paths recorded separately.

Local evidence: builddir/codex-2026-10-05/colorset-storage/. Source archives,
156 fingerprints, 1155 permission entries, initial/final gate logs, per-target
archives, configured-overrides.json, final-fixture-delta.json,
control-versus-final-binaries.json and guest cleanup records are retained.
