# Xlib dependency contract — 2026-10-21

LIB-05 / MAINT-02. Base: merged [PR143](https://github.com/gonz0w/fvwm-x/pull/143),
`fb3762e32d62d057389d68a66ea42c2a2929c943`.

## Selected contract

Remove the unused libxkbcommon development/link requirement. Existing key
translation uses `XkbKeycodeToKeysym` from libX11, so configure now compiles and
links that call with `X11/XKBlib.h` rather than asserting header availability.
Missing header or symbol produces an explicit configure error. Package recipes
for Linux, FreeBSD, OpenBSD and Nix, dependency inventory and INSTALL.md agree.
Xt and Xtrans remain required: their headers have actual native consumers.
No C implementation, input interpretation or FVWM configuration syntax changes.

This is one bounded stage 5 prerequisite, with its configure/package/failure
contracts kept together. Adding unrelated linkage, sandbox or interpreter
changes merely to reach three stories would combine independent acceptance
and ownership. The selected safety closeout is merged; broad audit, remaining
LIB-05 dependency/interpreter floors and other stage 5 owners stay open.

## Verification

Focused Linux ARM64 and FreeBSD ARM64 production-block controls pass. Each
executes the actual required-dependency Meson block with real development files:
libxkbcommon requests are refused, while missing-header and missing-symbol
controls must fail with the product diagnostic. These are configure/link probes,
not fake libraries or a copied policy implementation. Host macOS lacks Meson and
skips these controls; native results are recorded separately.

Combined six-target/compiler/install/X qualification is pending. Performance
reference PR138 is at 4/5 subsequent merged implementation checkpoints after
PR139/141/142/143; no comparison or performance-only tuning is due here.
Native-CPU Linux sanitizers, physical input/GPU/toolkits and release acceptance
remain separate. OpenBSD 8.0 is the current baseline; 7.9 stays out of scope.
