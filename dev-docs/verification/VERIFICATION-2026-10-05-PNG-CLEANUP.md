# PNG wrapper and verification-profile cleanup — 2026-10-05

Status: accepted focused checkpoint after the complete patched PNG baseline.
Owning items: IMAGE-01, CLEAN-01 and VERIFY-01. No rendering algorithm, theme
syntax or runtime library dependency changes in this cleanup.

## Change

The PNG-disabled decoder is now a separate stub. Its former header still carried
replica libpng/zlib structures, zlib aliases and no-op function macros with no
remaining callers. Remove those dead definitions: `libs/Fpng.h` drops from 499
to 71 lines while retaining the real types and legacy API aliases used by the
PNG-enabled implementation, including the older audited-backport API branches.
No external configuration or module packet API changes.

Linux/BSD verification entry points now pass explicit pkg-config paths to Meson
and clear cached dependency discovery on reconfiguration. A supplied BSD prefix
precedes standard directories. `FVWM_PNG_SECURITY_BACKPORT=true` forwards an
explicit builder assertion; omission defaults to false. It must only accompany
recorded vendor proof. These entry points do not provision or patch dependencies.

## Verification

All core/modules compile on Linux ARM64 GCC/Clang, Linux AMD64 GCC, and both
FreeBSD/OpenBSD architectures. Four focused Meson entries pass on each of seven
legs (28 executions): image safety, PNG constructor recovery, real-X resource
cache, and verification-profile arguments/defaults. The profile unit invokes the
entry-point arguments using recording tools and tests default false, explicit
true, requested path priority and cache-clearing reconfiguration. BSD's tool
PATH prelude is isolated in that unit; actual BSD dependency discovery is covered
by the real native configure/build/runtime profiles.

Staged installed cores pass 45 nested-X Linux image outcomes and 188 default-BSD-X
outcomes (image recovery plus previous text/widget checks). The PNG-disabled
Linux ARM64 build passes 46/46 non-live tests. Six archives match all 1175 frozen
source hashes/modes and exact Meson-transformed staged core/Script/MFL bytes.
This is focused follow-up coverage; it does not relabel the earlier baseline's
502 complete-suite executions as a complete suite for this later source.

An initial recording fixture selected its own interpreter recursively; the
fixture now uses an absolute interpreter and bounded subprocess timeout.
Its corrected host checks pass. An initial Meson registration used the wrong
Python variable; only that registration was corrected before native checks.
Failed gate logs remain archived. Neither issue was a production WM failure.

Evidence: `builddir/codex-2026-10-05/png-wrapper-cleanup/`, including frozen source,
scoped overlays, six evidence archives, default-server archives and validated
`accepted-results.json`. The full prior source/dependency gate is the
[PNG baseline report](VERIFICATION-2026-10-05-PNG-BASELINE.md). Native sanitizer CI
and wider decoder bounds/isolation remain open.

Checkpoint cleanup: the four owned snapshot guests are shut down and their exact
saved QEMU processes have exited. All accepted evidence was archived first.
Fresh BSD runs must provision the signed updates and patched private prefixes.
