# Interactive TextField ValueLimit — 2026-10-05

Status: approved by the user, implemented and verified. This checkpoint extends
ValueLimit to interactive TextField titles; it follows the separate
[text/list maintenance checkpoint](VERIFICATION-2026-10-05-SCRIPT-TEXT.md).

Typed insertion, middle-button paste and opt-in keyboard paste now use one
checked edit path and the existing ValueLimit (1 MiB default, configurable
1–2147483647 bytes). A refused edit preserves the title, cursor, selection and
viewport, logs a diagnostic without input contents and sends no successful
SingleClic. A selected replacement credits removed bytes. Deletion and shrinking
an older oversized title still work. Stored script values and MemoryLimit's
constant/variable scope retain their existing contracts. The obsolete private
InsertText implementation has been removed.

Ordinary unselected typing retains the legacy callback order in both keyboard
modes: the handler observes the pre-advance cursor. Successful paste retains its
existing ordering. This prevents the limit refactor from changing script handler
semantics. The manual documents the approved refusal and recovery behavior.

## Accepted gates

| Target | Full tests | Installed-code check | Installed default X |
| --- | --- | --- | --- |
| Linux ARM64 GCC | 77/77 | passed | nested X in suite |
| Linux ARM64 Clang | 77/77 | passed | nested X in suite |
| Linux AMD64 GCC | 77/77 | passed | nested X in suite |
| FreeBSD ARM64 | 66/66 | passed | 32 outcomes on Xorg |
| OpenBSD ARM64 | 66/66 | passed | 32 outcomes on wsfb |
| FreeBSD AMD64 | 66/66 | passed | 32 outcomes on Xorg |
| OpenBSD AMD64 | 66/66 | passed | 32 outcomes on Xenocara |

That is 495 suite executions and 128 installed default-server outcomes.
The expanded text/list fixture has 16 named outcomes: normal editing/recovery
plus exact-size, first-excess, whole-paste refusal, selection credit, unchanged
state and later recovery with 128/256-byte overrides in both keyboard modes.
The separate previous widget fixture retains all 16 outcomes. Native text policy
coverage includes the real 1 MiB boundary, aliasing, invalid sizes, overflow,
shrinking and pointer/content retention. Host Clang strict warnings and UBSan
also pass; native Linux sanitizer CI remains configured but unrun.

The source snapshot overlays eight owned files on the accepted text/list
baseline and excludes concurrent desktop/launcher/theme work. All six evidence
archives match 1173 source hashes and modes. Fresh staged core/Script/MFL files
match exact expected install bytes after applying Meson's recorded RPATH
transformation to private tested-binary copies. Installed version checks pass.
The four default-server follow-ups use those staged executables.

Fixture development initially treated WriteToFile's trailing blank line as a
title and assumed a different cursor observation for opt-in ordinary typing.
The readers were corrected; production preserves the existing callback order.
Those failed verification attempts remain in the archive. No production repair
was needed after the final gates began.

Evidence: `builddir/codex-2026-10-05/script-text-limit/`, including the frozen
source, six `*-evidence.tgz` archives, four default-X archives, full logs,
source metadata, package observations and validated `accepted-results.json`.
The snapshot QEMU guests are retained for the next image-loader/package batch;
original guest processes and concurrent desktop work are untouched.

## Remaining scope

This is a per-title bound. Aggregate widget memory, large-title font-width
complexity, complete Unicode handling, modal waits and wider resource budgets
remain open. Passing runtime tests does not qualify the installed third-party
packages as patched: the package/backport audit is a separate LIB-02/LIB-07
workstream. Go is disabled for this C runtime profile; the earlier Linux AMD64
Go emulation limitation and three-profile tooling evidence remain separate.
