# Native tray completion checkpoint — 2026-10-30

This checkpoint is qualified for review on `codex/r1-tray-completion`, based on
merged PR313 `db6b51ad29c515e06e0d2e0918eead2fc562d240`. The
[frozen contract](../decisions/TRAY-COMPLETION-2026-10-30.md) owns #78
acceptance. The [companion data](VERIFICATION-2026-10-30-TRAY-COMPLETION.json)
records source identities, archive hashes, exact inputs and measurements.
Affected qualification passes on all six profiles. Separate source, receipt and
visual review is complete. Issue closure belongs to merging the reviewed candidate. This is not
whole-release acceptance.

| Target / execution | Combined checkpoint | Final source binding |
| --- | --- | --- |
| Linux ARM64 / native CPU, Apple Hypervisor | Enabled/disabled build, C/Go, install, standalone/nested-X/Xorg and scoped sanitizer checks. | Source37 installed suite:17 groups; five required real GTK paint assertions. Source34 normal/scoped ASan icon correction; runtime input bytes unchanged. |
| Linux AMD64 / TCG | Source34 enabled/disabled builds, C/Go, affected installs, standalone/nested-X/Xorg pass. | Full checkpoint directly uses source34. |
| FreeBSD ARM64 / HVF | Source28 enabled/disabled build, C/Go, install, standalone/nested/default-X pass. | Source34 three-file icon build/install/policy/pixel delta passes; seven unchanged installed consumers compose with the baseline. |
| FreeBSD AMD64 / TCG | Source34 enabled/disabled builds, C/Go, affected installs, standalone/nested/default-X pass. | Full checkpoint directly uses source34. |
| OpenBSD8.0 ARM64 / HVF | Source29 combined enabled/disabled checkpoint passes. | Source34 three-file icon delta passes, composed with source29 and unchanged installed consumers. |
| OpenBSD8.0 AMD64 / TCG | Source29 combined enabled/disabled checkpoint passes. | Source34 three-file icon delta passes, composed with source29 and unchanged installed consumers. |

The current Linux fixture source closure is
`b1d4f2ab28f7e9978b120daf27aca6e2ac07511e14ab898f5a40f57cf62d8086`.
The affected installed executables use the source 34 production runtime; the
fixture changed afterward. Independent peers and capture helper use the build
tree, so this is not a whole-installed-desktop qualification.
Its actual X/bus run covers both dialects, menus, ownership and recovery,
container behavior, three theme configurations and GTK 3.24.41/Ayatana 0.5.93
clients. Synthetic protocol fixtures are distinct from real clients. The
[gallery](../../doc/development-preview.md#native-tray-completion-work-in-progress)
shows inspected captures; images do not prove interaction or accessibility.
The earlier GTK black capture followed a PNG load failure with isolated MIME
data. A loader probe identified the same PNG as octet-stream there and decoded
it with base MIME paths. The corrected fixture producer uses those base paths;
Five required blue/orange paint assertions passed after show, hide/show,
update and reposition; a separate initial stage records observation only. Guest
and synthetic-X results do not qualify physical hardware.

The final Linux sample measured modern-off first ready at 0.0775 seconds and
modern enable to connected at 0.0246 seconds. In approximately one-second
sequential samples, tray RSS was 7,324 KiB with modern off, 9,360 KiB with two
named icons ready and 9,384 KiB with 31 modern entries plus one XEmbed entry;
tray CPU ticks did not change within each sample. These are observations of
one profile, not latency, peak memory or system-wide bounds. The JSON retains
core, descriptor, executable and sampling details. Cooperative work budgets do
not preempt a blocking library, kernel or X call.

The modern adapter uses the existing optional libdbus feature, off by default;
the disabled Tray has no D-Bus linkage. Optional XFixes supports containment on
both owners. GTK/Ayatana are test-only clients. Icon acquisition reuses the
existing Go worker and resolver. In the recorded Linux ARM64 compile profile,
installed unstripped Tray files are 1,912,376 bytes enabled and 1,258,152 bytes
with D-Bus disabled. File size is separate from resident memory and does not
establish causal feature overhead or a stripped-package size.

Failures remain evidence. FreeBSD platform03 failed waiting for a nested menu
reopen; platform04 passed at its later source. OpenBSD ARM64's first driver
failed and the corrected second driver passed; an internal pass does not turn
the failed outer run into a pass. Linux attention03 failed waiting for the
half-size overlay paint; attention04 passed after correction at source 34. See
the retained result files and archive hashes in the companion data. The source 36 toolkit01 run failed with an empty GTK image and direct PNG
load error; the MIME probe and source 37 fixture correction resolved that
fixture failure. Earlier failed attempts are not erased by later observations.

Four generated Linux roff pages were byte-equal to the installed pages and
rendered with macOS mandoc at exit 0. Lint was not clean: Tray, Buttons and
Desktop exited 3; MFL exited 4. Saved lint diagnostics are classified: shared Asciidoctor `.mso www.tmac`
and `.LINKSTYLE` compatibility errors, generated long lines and converter spacing;
MFL additionally reports 130 unsupported table-cell spacing macros. Saved rendering
retains the table text and borders. These are nonzero lint results, without a
claim of baseline equivalence. Final FreeBSD and OpenBSD native rendering
exited 0 for all four pages, with lint exits 3,3,3,4. Full Buttons font-lifetime
leakage remains the finite R2
follow-up. The periodic performance counter is 3 of 5 after merged PR313;
no paired comparison is due. Scoped sanitizer results
here do not establish a clean full-session leak run or baseline parity.

Only three production files changed between the OpenBSD source 29 baseline
and source34; the retained 55 C/Go/build input tuples bind that comparison.
Both OpenBSD final deltas pass. The separate review is recorded below.
Backend role
routing was not verified.

The first OpenBSD delta launcher failed before its icon gate because it compared
the resolved cached `/home/fvwm-ci` prefix with an unresolved `/root/fvwm-ci`
alias. Resolving both operands fixes the launcher; both corrected jobs pass.
The superseded AMD64 preparation was cancelled through its owned runner before
that known-invalid launcher ran. Failed/cancelled records remain distinct from
the corrected results.

All owned labs are stopped. Verified build/evidence archives and useful persistent
profiles remain. Retired owned disposable disks and duplicate pinned image copies
total 38,507,724,857 bytes across the recorded cleanup passes; shared asset caches,
SDKs, the runnable performance reference and other owners’ resources are preserved.

Separate review examined artifact tree `7d52850750d67702b91059c9b914b27f80d840ee` and
its exact patch. The only finding was a stale provenance sentence, corrected to
52 unchanged inputs plus three qualified SNI inputs. Completion metadata follows
that examination; the signed PR head identifies the delivered source.
