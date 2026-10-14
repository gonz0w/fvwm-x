# PNG loader error recovery — 2026-10-05

Status: recovery and callback-initialization repair verified on six platforms
and the Linux Clang leg.
Third-party package patch qualification remains a separate open gate.

## Finding and repair

An ordinary incomplete PNG containing a signature and IHDR but no image data
terminated the unmodified production-linked image probe with SIGABRT (exit -6,
`libpng error: Read Error`). The loader's error-recovery block was disabled and
used an obsolete direct struct member. This is a reproduced availability issue;
no advisory-specific input or exploitability claim is needed.

The loader now installs a nonreturning error callback and uses a standard C jump
buffer. Mutable allocation ownership lives in a small heap context, so cleanup
can safely read it after longjmp. Errors during metadata, pixel and end-chunk
reading free owned rows/pixels, destroy libpng state, close the file and return
failure. The diagnostic does not include image contents. Short signatures are
checked before inspecting their bytes. The optional PNG-disabled implementation
is a separate small stub; no new runtime library or API floor is introduced.

Construction initially registered the callback before the application jump
destination existed. Follow-up source review moved registration after setjmp;
libpng construction uses its own recovery. A native wrapped-call regression
repeats read/info allocation failures and an error at callback installation,
checking cleared outputs, cleanup calls and descriptor stability.

The public pixmap loader previously assigned None to local pointer variables
on failure. It now clears the caller's pixmap, mask and alpha outputs and resets
allocation/geometry flags. Valid color/alpha transforms, scaling, format order
and cache ownership retain their previous behavior. The color flags use the
already-read IHDR type instead of repeated getters and obsolete commentary.

## Verification

| Target | Full suite | Installed default X |
| --- | --- | --- |
| Linux ARM64 GCC | 78/78 | 15 staged core/image checks on nested X |
| Linux ARM64 Clang | 78/78 | 15 staged core/image checks on nested X |
| Linux AMD64 GCC | 78/78 | 15 staged core/image checks on nested X |
| FreeBSD ARM64 | 67/67 | 47 outcomes on Xorg |
| OpenBSD ARM64 | 67/67 | 47 outcomes on wsfb |
| FreeBSD AMD64 | 67/67 | 47 outcomes on Xorg |
| OpenBSD AMD64 | 67/67 | 47 outcomes on Xenocara |

502 full-suite executions, 188 installed default-BSD-X outcomes and 45 staged
Linux core/image checks pass. A supplemental Linux ARM64 GCC PNG-disabled build
passes all 45 tests in its non-live suite. Six archives match 1174 source hashes
and modes, and staged core/Script/MFL files match exact expected Meson install
bytes after the recorded RPATH transformation. Go is disabled in this runtime
profile; native Linux sanitizer CI remains configured but unrun.

The permanent resource-cache fixture covers six ordinary incomplete/CRC-invalid
files. Each file is refused repeatedly through both picture APIs, with cleared
outputs, unchanged low-descriptor count and successful later valid cache loads.
Existing image/font cache scale, identity, metadata and destruction checks pass.
Descriptor checks cover 0–255 and are not a complete resource-leak proof.

The saved auxiliary core fixture uses colorset image commands to exercise the
actual WM: after each refusal it accepts a valid image and command barrier,
reports a recoverable diagnostic and manages a later client. Every BSD
follow-up runs staged core/modules against its default server and the
production-linked native probe, plus the prior 32 text/widget outcomes.
This auxiliary fixture is archived verification tooling outside the frozen
source manifest; the permanent regression is registered in the existing suite.

The first compile of the refactor missed a conditional-compilation terminator;
that source is retained under `compile-correction/`. The corrected candidate
also removed newly introduced clobber warnings. An auxiliary XBM fixture first
used a line layout its parser did not accept; the corrected fixture and failed
artifact are retained. The initial frozen candidate passed its runtime gates. The initialization
follow-up is separately frozen and fully qualified. An initial follow-up freeze
accidentally included concurrent Meson targets; its correction overlays only
the owned test registration and preserves the failed logs.

Evidence: `builddir/codex-2026-10-05/png-error-setup/` contains the accepted
final source and all follow-up gates. `image-loader/` retains before/control
artifacts, frozen source, six full evidence archives, four default-X archives,
extra installed-core checks and validated `accepted-results.json`. Snapshot
guests are retained for signed-package refresh and revalidation; original
processes and concurrent desktop source are untouched.

## Practical limits

Error recovery does not repair bugs inside libpng, XPM, SVG or transitive
libraries. It does not isolate decoders, establish file/CPU/resource budgets or
make synchronous image work nonblocking. The package audit records old or
unverified dependencies separately. Native sanitizer/failure-allocation coverage,
unused ancillary-chunk processing, image bounds and ownership remain open.
See the [package audit](../DEPENDENCY-AUDIT-2026-10-05.md).

## Callback initialization follow-up

Final source review found that registering the custom callback during read-struct
construction precedes the application's setjmp initialization. The follow-up
uses the standard library construction path and registers the callback after
its destination is armed. New native fault injection checks read-struct and
info-struct allocation failure and an error at the first callback-installation
point, repeated with closed descriptors, reset outputs and correct destruction.
Focused Linux ARM64 checks pass; final cross-platform gates are running.
The initial recovery snapshot remains archived separately.
