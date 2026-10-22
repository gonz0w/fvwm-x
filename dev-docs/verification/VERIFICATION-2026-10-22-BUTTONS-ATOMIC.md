# Atomic grouped button updates — BUTTON-02

Implemented; focused Linux policy, native nested-X and existing transport checks
pass. Combined qualification is pending. This checkpoint finishes one coherent
button update contract through three related behavior improvements:

- Bounded whole-group parsing and validation, including duplicate selectors,
  properties, exact numeric selectors, values and existing colorset references.
- Operation-owned string/icon preparation and complete resource cleanup on refusal.
- Commit every property before final-state focus/held-key reconciliation and one
  bounded repaint pass; preserve IDs, actions, window identity and legacy
  incremental ChangeButton behavior.

The additive ChangeButtons command accepts at most 8192 bytes, 32 buttons and
128 changes. It is a logical per-module transaction, not a display-frame or
whole-WM transaction. Image loading remains synchronous within the isolated
module and follows existing image limits. Existing FVWM configuration syntax
and commands require no migration or new runtime dependency.

Qualification will compose fresh affected module/unit/X/install/default-BSD-X
checks across six targets/ten profiles with PR149's retained broad coverage only
after exact frozen-source, generated configuration and unchanged installed
consumer/data identities are proven. Core geometry and unrelated full suites
are not rerun solely for this module change. Scoped guest ASan/UBSan includes
leak checking. Native-CPU sanitizer, hardware/toolkit/compositor and release
acceptance remain separate.

The original command-prefix collision was exposed by the first actual-X check
and corrected by matching the longer additive command first. The failed capture
is retained alongside passing focused checks. A local macOS warning-as-error
compile refused the test's sprintf calls; they now use bounded snprintf.

Canonical owner artifacts:
`builddir/codex-2026-10-22/buttons-atomic-updates/`.
The due five-checkpoint [performance comparison](PERFORMANCE-2026-10-22-PR149.md)
uses existing PR144/149 binaries, independently of this feature candidate.

The first frozen checkpoint retained successful standard-profile, install and
OpenBSD default-display feature results, but the icon fixture incorrectly
required optional XPM support in Linux minimal and FreeBSD profiles. Its
portable correction uses X11's base bitmap loader and passes focused Linux
checks. Product C/header source is unchanged. The OpenBSD ARM64 auxiliary
PNG proof encounters a retained directory name from the verified object cache;
the final runner uses fresh names. All original captures remain preserved.
The final affected selection also includes every registered test consuming
FvwmButtons (desktop, fonts, session and packet/service/partial transport),
with unavailable minimal-profile registrations recorded explicitly.

Source review adds an in-grid check before the existing coordinate-to-cell
multiplication. Large but int-representable coordinates are refused before
arithmetic; actual-X regressions cover both large coordinates and row-spilling
column aliases. The undispatched fixture-only candidate is retained, and the
final product candidate includes this correction.

The original OpenBSD AMD64 fixture reaches its 90-second deadline while making
per-pixel Python/FFI calls for whole-window rollback comparisons. The packed-image
fixture preserves every compared pixel with one bounded buffer copy; focused
Linux checks pass against the exact compiled product. The final affected-consumer
checkpoint is retained with its original outcomes. Packed-fixture follow-ups
verify unchanged module bytes and replace only affected fixture outcomes;
unrelated consumer checks and broad suites are not repeated.
