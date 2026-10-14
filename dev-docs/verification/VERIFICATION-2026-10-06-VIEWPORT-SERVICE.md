# INPUT-01 active viewport interaction — 2026-10-06

Status: locally accepted. Eight compiler profiles pass 952 test entries, 120
installed code identities and 16 default-BSD-X driver invocations. This subsequent checkpoint is separate from common button-release gates.

The user approved the [viewport contract](../decisions/VIEWPORT-INTERACTION-SERVICE-PROPOSAL.md).
Four related stories reuse existing single-threaded interaction ownership:

| Story | Result |
| --- | --- |
| Finite readiness and cancellation | Scroll reuses the 100-ms interaction wait and common cleanup for pointer/keyboard grabs. SIGTERM ends active Scroll even with a physical button held. |
| Motion fairness | Existing 32-event coalescing replaces an unlimited drain and corrects the release-mask typo. Each bounded batch services the existing audited command/timer/transport pump. |
| Ordering and borrowed state | Safe independent producers/timers progress; active producer continuations, unsafe heads and synchronous receiver replies retain order. Captured monitor records, lifecycle dispatch and image-owner publication stay behind outer unwind. No new scheduler or mutating-command permission. |
| Checked button arithmetic | One pure validated button-mask helper replaces zero-capable shifts in Scroll, Move and Resize and shares final release-mask validation. Zero/oversized buttons have no mask; valid button behavior stays unchanged. |

## Focused acceptance

Evidence: `builddir/codex-2026-10-06/viewport-service/` and the isolated Linux
source `/tmp/fvwm-button-release-source`, `verification-viewport-focused/`.
The eight-entry checkpoint passes policy, existing held Move/Resize, their final
release waits, common release callers, Scroll/reverse producer-order tests,
viewport coordinates/keyboard completion on Xvfb and Xorg-dummy, and viewport
image completion/deadline/shutdown boundaries. Actual RandR mode changes remain
uncommitted through active scrolling and the final held-button wait; the topology
epoch advances after unwind. A new client's MapRequest also remains queued until
release. Escape and Return keep function continuation behind physical release.

Fixture repairs are retained: a reused client binding was corrected to root;
a root click's own final wait preceded the intended active module command, so
that case now starts Scroll from the module with no physical button and verifies
continuation order until Escape; the Escape helper asserts a remaining grab only
when a physical button is held. Coordinate checks declare one EWMH desktop
explicitly rather than assuming an installed defaults file. These repairs did
not change production behavior. The ordinary root-held cases still verify
physical-release ordering. No performance benchmark is claimed.

The frozen source has 1263 files, SHA-256
`6b0e8a9091bca05bd19bf4ee37e31e588bb9d8f7fb0ebb42c18f67954359c80c`.
It extends the frozen common-release checkpoint with ten owned code/test/manual
files and excludes unrelated desktop/theme/runtime-language work. All seven collected evidence archives match the frozen file hashes and modes.
The software-rendering optional profile passes 124 entries, SVG-disabled passes
105, and scoped ASan/UBSan passes nine entries: one policy with leak detection
and eight actual-core checks without leak detection.
Native Linux release sanitizer CI and physical toolkit/compositor checks remain
separate unrun requirements.


## Combined checkpoint evidence

`builddir/codex-2026-10-06/viewport-service/qualification-summary.json` validates
all collected payloads to EOF, registered test names, source identities and
installed/tested byte identities after only declared RPATH fixups. Linux AMD64
and ARM64 each pass GCC and Clang (125 entries per profile); all four BSD targets
pass Clang (113 entries each). Four installed-core default-X drivers pass on each
BSD target: forward/reverse Scroll servicing, coordinate/keyboard/lifecycle
behavior, and image/termination boundaries. Default X server logs are retained.

OpenBSD AMD64's first registered run passed 112 entries; the verification
harness's own `verification-profile` test exceeded its 30-second limit. Only that
test was rerun on unchanged source with the original timeout and passed.
`native-initial-testlog.json`, `native-profile-rerun-testlog.json` and the validated
113-row composition retain exact provenance; the original failed log remains.
Its first default-server driver also missed the pointer-coordinate predicate.
The complete failure is retained in `default-x-viewport-initial-pointer-failure`;
the unchanged-source rerun passed all four drivers. The fixture's root binding
can expose a passive grab before Scroll records its pointer origin. A serviced
command handshake is added only in the subsequent menu checkpoint to remove
that race; it is not represented as part of this frozen source.

The active-menu production changes and unrelated shared-checkout work are absent
from this checkpoint. Native Linux release sanitizer CI and physical desktop,
toolkit/compositor checks remain unrun. These emulated/local results do not
substitute for them.
