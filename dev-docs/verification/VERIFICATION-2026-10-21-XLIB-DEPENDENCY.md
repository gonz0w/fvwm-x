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

This is one independently qualified packaging contract at the stage 5
dependency handoff. Changing a mandatory dependency affects the existing
configure, package and install profiles, so its success/refusal evidence stays
together. The selected safety closeout is merged; broad audit, remaining
LIB-05 dependency/interpreter floors and other stage 5 owners stay open.

## Verification

Focused Linux ARM64 and FreeBSD ARM64 production-block controls pass. Each
executes the actual required-dependency Meson block with real development files:
libxkbcommon requests are refused, while missing-header and missing-symbol
controls must fail with the product diagnostic. These are configure/link probes,
not fake libraries or a copied policy implementation. Host macOS lacks Meson and
skips these controls; native results are recorded separately.

The combined checkpoint is locally qualified. 2284 standard / 33 selected
optional entries pass across six targets/ten profiles; 30 native configure
controls, ten fresh installs and four authenticated BSD displays pass.
Generated configuration hashes remain exact; dependency introspection and
emitted link commands contain no direct libxkbcommon requirement.

| Gate | Passed / observed |
| --- | --- |
| Linux ARM64 and AMD64, GCC and Clang | 292 standard entries per profile |
| FreeBSD and OpenBSD 8.0, both architectures | 279 standard entries per target |
| Linux ARM64 software/minimal | 33 selected entries combined |
| Production dependency block | 30 controls across ten profiles; six earlier focused controls |
| Fresh install identities | 150 ELF, 77 catalogs, 700 data files |
| Staged real-X | 30 function groups, 20 geometry groups at 1x/1.5x, ten style groups |
| Authenticated BSD default X | 32 groups on four displays; unauthorized clients refused, servers exit 0 |
| Portable `make verify` | 177 passes / 14 capability skips, one run |

All 2,118 frozen regular source inputs pass native hash/mode postchecks; captured
archives pass EOF/CRC, payload hashes, result and source identity validation.
Generated config.h remains byte-identical in all ten profiles. Loader receipts
record actual staged core dependencies; removing a direct dependency makes no
claim about every distributor's transitive desktop packages. Native Meson
introspection and emitted Ninja link commands independently confirm removal.
No C/header implementation changes from the merged base. No product test
failure or retry was needed. Package-shell syntax and local source/consumer
review pass. Nix is updated textually; no Nix build is claimed. The final
documentation guard caught a 252-word README status; shortening it to 250
requires no runtime repetition. The original guard log remains preserved.

Signed author-DCO implementation: `1d5b8d080555c11890d4d66ff225a192a5f2389a`.
Frozen source SHA256:
`4b4e19b0788517f06005fb498cec2894f8e9509eadfb87013fc7c44586685f65`.
[Machine receipt](VERIFICATION-2026-10-21-XLIB-DEPENDENCY.json) binds targets,
commands/results, generated-config parity, dependency observations and final
documentation composition. Canonical raw evidence:
`builddir/codex-2026-10-21/dependency-contracts`. Final upkeep changes Markdown
and the receipt only. Completed guest evidence duplicates and temporary host
build-cache copies are retired after canonical verification; reusable builds,
source, install/performance references and VM disks stay intact.

Memory/lifetime code is unchanged, so no extra sanitizer rebuild is performed;
[PR143's sanitizer evidence](VERIFICATION-2026-10-21-FUNCTION-LIFETIMES.md)
retains its source/profile scope. Native-CPU sanitizer, physical input/GPU,
third-party toolkits/compositors and final release acceptance remain separate.
Remaining LIB-05 dependency/interpreter floors and other stage 5 owners stay
open. OpenBSD follows new releases; 8.0 is the current qualified baseline and
7.9 remains out of scope.

Performance reference PR138 is at 4/5 subsequent merged implementation
checkpoints after PR139/141/142/143. No comparison or tuning is due in this
unmerged PR; after PR144 merges the next periodic comparison is due, reusing
existing binaries rather than running another platform matrix.
