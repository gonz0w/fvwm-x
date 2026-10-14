# Image acquisition and compressed-XPM limits — 2026-10-05

Status: accepted bounded checkpoint for IMAGE-01 / DATA-01 / PROC-01. Broader
decoder isolation, SVG secondary-resource limits and native Linux release
sanitizer CI remain open. This completes production adoption of the
[reader preparation](VERIFICATION-2026-10-05-IMAGE-INPUT-READER.md).

## Approved behavior and implementation

The user approved regular-file primary images, a 128 MiB source-byte cap,
128 MiB expanded compressed-XPM cap, and a five-second whole-decompression
deadline. Finite Meson overrides are 1–1024 MiB and 1–60 seconds. `image-gzip`
selects a trusted literal executable path; no image name becomes shell text.

The shared loader now opens and checks the actual descriptor once, counts bytes
and hands a bounded snapshot to PNG callbacks, XPM buffers, SVG streams,
Xcursor callbacks and a narrow XBM buffer adapter. Special-file primary sources
are refused without entering a blocking FIFO open. Regular symlinks and hard
links remain usable. Source metadata comes from the opened inode; later path
replacement cannot substitute a different decoder input or cache stamp.
This is a collected byte buffer, not an atomic snapshot of a concurrently
modified filesystem file. Actual growth and short reads are checked.

Compressed XPM uses literal gzip arguments, nonblocking counted duplex pipes
and one monotonic deadline covering startup, transport and child completion.
Failures discard output and terminate/reap the owned process before losing its
identity. Inherited descriptors are closed; caller signal state and unrelated
BSD child notifications are preserved. The shared SIGPIPE-safe write helper
also replaces FvwmScript FIFO's previous private copy.

Decoder ordering, ordinary theme paths, scaling, color/alpha, cache generations,
SVG relative resources and `:options:path` syntax remain. Missing bare XPM names
retain checked `.Z`/`.gz` fallback. XBM parsing preserves Xlib's legacy X10/X11
syntax and wide file geometry; the X server's independent pixmap limits still
apply. Unsupported SVG correctly returns failure so bitmap fallback proceeds.
No new mandatory image library or desktop package is introduced.

Real-X/sanitizer qualification also found and fixed inherited image cleanup:
copy cursor pixels into Xcursor-owned storage instead of freeing a borrowed
buffer twice; avoid freeing a nonexistent opaque-cursor mask in Xcursor-disabled
builds; release successful XPM decoder storage and check color indices; pack
cursor/SVG color words with unsigned shifts. Public failure outputs are cleared.

These limits replace existing synchronous acquisition/decompression paths.
They do not make all storage access or library parsing nonblocking, and a
five-second deadline alone does not meet the desktop responsiveness goal.

## Final frozen-candidate gates

All core C executables and modules compile on the following seven compiler
legs. Go is disabled for this C/image checkpoint. Each final configured suite
passes; these are **537 suite executions** in total.

| Target | Compiler | Final full suite |
| --- | --- | --- |
| Linux ARM64 | GCC 13 | 83/83 |
| Linux ARM64 | Clang 18 | 83/83 |
| Linux AMD64 | GCC | 83/83 |
| FreeBSD ARM64 | native Clang | 72/72 |
| FreeBSD AMD64 | native Clang | 72/72 |
| OpenBSD ARM64 | native Clang | 72/72 |
| OpenBSD AMD64 | native Clang | 72/72 |

Pure fixtures cover opened-inode identity, type/size boundaries, growth/shrink,
allocation/read/close failures, output ownership, XBM parity, decompressor
limits/deadlines, stalled/early/error children, cancellation, descriptor closure,
SIGPIPE state and unrelated child delivery. Real-X tests use actual pixels,
cursor hotspots and animation frames rather than merely checking return codes.

Staged installed cores pass **192 Linux nested-X image outcomes** across the
three compiler legs. The four installed BSD default X servers pass **232 image
outcomes plus 40 FvwmScript FIFO outcomes**, including continued command/window
operation after refused inputs. Counts vary with actual optional formats and
base-system Unix `compress`: FreeBSD passes 49 image outcomes per architecture;
OpenBSD passes 67. FreeBSD profiles have no XPM library; SVG, PNG and Xcursor
are enabled. OpenBSD exercises all those formats plus XPM and true `.Z` input.
Linux passes 64 per compiler leg. Earlier fixture runs in the archives are
retained but excluded from these final counts.

Actual default drivers in this checkpoint are FreeBSD `scfb` on both
architectures, OpenBSD ARM64 `wsfb`, and OpenBSD AMD64 `vesa`; their archived
server logs report depth 24. These are guest default Xorg/Xenocara checks,
not a claim about all physical graphics hardware or earlier depth-16 profiles.

Linux ARM64 optional builds pass seven focused entries with Xcursor disabled
and five with PNG/SVG/XPM/Xcursor/XRender disabled. The latter exercises actual
bitmap fallback with 1 MiB source/expanded limits and a one-second deadline.
The scoped Clang ASan/UBSan build passes **4/4** with leak detection and
halt-on-error, covering the three pure helpers and real image/cursor loading.
This local virtualized run does not replace still-unrun native Linux release CI.
Clang's existing checked `__int128` arithmetic required its installed compiler-rt
builtins link option; the accepted command uses explicit Meson `c_link_args`.

Six archived targets match **all 1186 frozen source fingerprints and modes**.
All **14 installed native binaries per compiler leg** match the expected exact
Meson RPATH transformation of the tested binary: 98 identities checked. The
installed versions, actual loader paths, compiler/platform metadata, test logs,
optional profiles and sanitizer environment are archived. Patched PNG profiles
and the audited Debian PNG/XPM backport match the accepted
[dependency audit](../DEPENDENCY-AUDIT-2026-10-05.md); signed OpenBSD updates and
qualified private PNG prefixes were restored in these disposable snapshots.

The frozen archive starts from the previous accepted reader checkpoint and
overlays only this batch's runtime files and scoped Meson/manual additions.
Concurrent desktop/tooling edits in the shared checkout are preserved and
excluded from this candidate. Exact owned runtime bytes still match the root
checkout. This report does not qualify unrelated later edits.

## Failures retained and manual review

Initial native checks exposed BSD's separate pending SIGPIPE queues; the fixture
now consumes its owned pending signals before unblocking. A later unrelated-child
fixture exposed a real SIGCHLD delivery problem: changing a normal handler to
default discarded pending BSD notifications. The implementation now retains
the normal handler and only changes ignored/automatic-reap dispositions as
needed. Both corrected tests pass on all final legs.

A wide XBM parser parity fixture succeeds; trying to turn the same wide bitmap
into a server pixmap hit BSD X `BadAlloc`. The file-format parity check remains;
the inappropriate server-geometry fixture was removed. Optional and sanitizer
failures led to the actual cursor/XPM/unsigned-packing repairs described above.

One intermediate FreeBSD ARM64 full run failed the unchanged interaction-servicing
sync-reply timing assertion. Three isolated repeats passed without code/fixture
changes, and the final full suite passes. This remains an unexplained intermittent
observation under VERIFY-01, not a claimed interaction fix. Earlier failed gate
logs and retry results remain in the evidence folder.

The existing AsciiDoc source documents all four image build options. Generated
roff was actually rendered with native BSD mandoc and the displayed limits were
checked. Baseline and candidate have the same two inherited Asciidoctor macro
errors and 33 warnings; STYLE source-line-length notes increase from 22 to 25.
Terminal wrapping is reviewed; **mandoc lint is not clean** (exit 3 for both).
This is a focused manual update, not the deferred manual/website rewrite.

## Remaining SVG decision and evidence

Against the exact final staged Linux ARM64 core, a regular SVG referencing a
task-owned secondary FIFO still blocks the following command barrier for the
controlled two-second observation. The live core is in `wait_for_partner`.
Harness cleanup completes. The primary-file and compressed-XPM limits do not
cover librsvg secondary opens, SVGZ expansion or rendering CPU/memory.

The [SVG isolation proposal](../decisions/IMAGE-SVG-ISOLATION-PROPOSAL.md) records
the separately approved compatibility policy for a five-second SVG operation deadline and
1024 MiB worker memory limit with finite overrides. Ordinary relative resources
remain supported. The user approved that recommendation after this checkpoint; worker implementation
is a separate batch. No worker or new SVG limit is implemented by this checkpoint.

Evidence: `builddir/codex-2026-10-05/image-input-integration/`, including
`source-final.tgz`, source hashes/modes, six evidence archives, four installed
default-X archives, complete server logs, `accepted-results.json`, manual review,
failed attempts and the final bounded SVG observation. Owned snapshot release
is recorded separately in `guest-release.json` after archival; original guests
and concurrent work are outside this task.

Checkpoint cleanup completed: all four owned BSD snapshots were shut down and
only processes matching their saved exact QEMU argv were released.
`guest-release.json` confirms no remaining matches and preserves original guests.
