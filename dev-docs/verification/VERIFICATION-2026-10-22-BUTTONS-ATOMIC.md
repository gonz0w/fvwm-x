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
