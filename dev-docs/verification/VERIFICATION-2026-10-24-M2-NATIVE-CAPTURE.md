# M2.5: native desktop screenshot capture

After Cam's explicit merge/cleanup instruction, PR195 is squash-merged as
`ce9916de2eed0a2556e653497673f8a73352b256`. GitHub verifies the canonical
signature; its tree matches the reviewed candidate and the author DCO remains.
The completed local/remote branch is removed. Broader #23/#7/#15 remain open.

Replace the isolated Python capture helper with C Xlib/libpng, without a new
library, toolkit or Go binding. The process owns its X connection, error policy,
image, one RGB row and private output. It preserves Print/module commands,
unique `FVWM-X-*.png` files, 0600 permissions, TrueColor channel scaling and the
16-megapixel limit. Geometry is admitted before XGetImage; validated layout and
contiguous/disjoint masks precede pixel use. Reuse `FImageSizeIsSafe` for pure
layout policy. Encoding/allocation/file refusal removes only the new partial
output; the resident still owns its existing deadline/process group. An abrupt
external kill is not a completed-write or rollback guarantee.

The helper links its selected X11/libpng dependencies directly, without the
aggregate UI dependency set. A PNG-disabled profile builds an explicit refusal
stub. Installed controller/worker/capture are ELF executables and require no
Python. This is not an interpreter-free distribution: Perl framework and
build/generator/test tools retain their own owners. The wallpaper generator keeps
its exact prior PNG-function AST locally; runtime capture is no longer imported
by tooling. Existing independent screenshot consumers now invoke the executable.

## Focused evidence

[Machine receipts](VERIFICATION-2026-10-24-M2-NATIVE-CAPTURE.json) bind 1,593
runtime/build/test/manual inputs and all eight preserved phase/target archives.
Canonical source, complete gzip/payload hash/size/mode checks and screenshots
are under `builddir/codex-2026-10-24/m2-native-capture/`.

- Linux ARM64 GCC and FreeBSD 15.1 ARM64: incremental capture/resident builds,
  pure geometry/mask/RGB policy, two Go entry/lookup cases, six independent
  installed module/CLI peers, ordinary build-tree dispatch and actual-X capture
  pass. Known 16/24/30-bit root pixels decode exactly through an independent PNG
  CRC/filter/RGB reader. Duplicate names stay unique/private; missing display,
  indexed visual, invalid arguments and occupied-directory refusals pass.
  A 32-byte file-size quota proves failed output removal without a success notice.
- Linux's existing desktop/settings/report/services flow and three representative
  theme profiles pass, producing 28 review images. Slate dark, Dune light and
  classic 9x15/Form/border settings views are inspected: readable controls, no
  clipping. The fixture's transient window and background screenshot notice are
  retained. There is no new UI feature or cosmetic retake.
- A separately compiled Linux ASan/UBSan helper repeats actual-X RGB/refusal/
  cleanup checks with leak detection. This scoped guest pass is not native-CPU
  qualification. Final GCC `-O2 -Wall -Wextra -Werror` compilation passes.
- Final tagged installs and an actual module→adjacent helper capture pass on
  both systems. All code sections match the build executable. Meson rewrites
  `.dynamic` on Linux and `.dynamic`/`.dynstr` on FreeBSD as expected; linkage
  receipts retain the selected patched libpng. The standalone disabled-PNG
  compile/refusal passes on each target; no full reduced-install claim follows.
- Changed manual rendering, repository checks/lint, Python syntax and Go format
  checks pass. Existing core/config/MFL/Form/Buttons hashes stay exact. Unchanged
  resident/worker policy and race coverage remains source-bound to PR195.

All captured commands take 47.299 seconds in aggregate, including scoped
follow-ups; transfer, VM startup and human/code review time are separate. Reuse
one incremental compiler profile and the affected BSD guest. No six-target,
full-install/default-server or unchanged Go-policy matrix repeats.

## Review corrections and remaining work

The initial host policy compile intentionally fails before its implementation.
The first Linux build passes with a GCC `setjmp` clobber warning. Lead review
makes the success flag volatile; final C policy/RGB/refusal and scoped sanitizer
checks repeat on the affected targets. Every other source/mode is exact, so
native UI/theme, module peers, Go lookup and manual results are retained.

A supplementary staging check initially assumes raw build/installed ELF hashes
must match and refuses both installs. Section comparison identifies only Meson's
runtime-path metadata rewrite. Correct that evidence assumption, then run final
installed dispatch on both targets without changing product code or weakening
code-section identity. The failed attempts remain in canonical archives. The
sandbox's initial local-Podman connection denial occurs before dispatch; the
permitted existing connection then runs the checks.

Retire 245 exact guest capture/input/archive duplicates (49,253,154 bytes) only
after host integrity and payload verification. Canonical host sources/evidence,
reusable source/build/cache/install stages, disks and other owners remain.
The owned FreeBSD guest shuts down normally after collection/cleanup.

Next: complete the remaining M2 runtime inventory, Python-free installed/session
measurements and justified owner/platform acceptance before M3. This checkpoint
does not close broad #23/#7/#15 or release/hardware/toolkit/compositor gaps.
PR194's performance reference is 1/5 after PR195, 2/5 on this merge; no periodic
comparison is due, and no controller/session speed improvement is claimed.
