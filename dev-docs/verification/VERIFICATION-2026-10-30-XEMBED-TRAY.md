# Standalone XEmbed tray development checkpoint

The first opt-in `FvwmTray` host passes focused native Linux build/model,
installed-module, actual-X and scoped sanitizer checks. This completes the
selected standalone slice of **TRAY-01 #78**, not the whole issue or R1 release
qualification. PR311 merged as `2dcf567312420f9aa524f72d42bfc3c1d67779b9`;
its signature, reviewed tree and author DCO were verified, and #76 is closed.

[Machine-readable receipts and limits](VERIFICATION-2026-10-30-XEMBED-TRAY.json)
record source identity, commands, actual inputs, observed cases and retention.
The executed source closure is
`208a183f54a0cf010f25502c4a7cf9b44774fc65f74bea5592d7129c3c0383c3`.
Final documentation, gallery and tracker edits follow that runtime freeze and
change no native input. The signed candidate and separate final review identify
that complete artifact in the PR.

## Delivered behavior

- A timestamped, non-replacing per-screen tray selection and MANAGER announcement.
- At most 32 independently owned clients, each 1..256 actual pixels per axis,
  with validated properties, transactional admission and ordered prefix overflow.
- Version-zero XEmbed embedding, mapped flags, resize, activation, explicit focus,
  key forwarding and next/previous traversal. Host fonts/spacing/geometry scale;
  application pixels do not.
- Live atomic configuration, colorsets, font, orientation, placement, visibility,
  primary-monitor and published work-area updates.
- Client destruction, external reparenting, producer disconnect, selection loss,
  graceful root release and abrupt standalone-host save-set recovery/redocking.
- Native build/manual/install integration, independent model and X client probes,
  and [three theme/font previews](../../doc/development-preview.md#standalone-xembed-tray).

The standalone host is an override-redirect root child, with private sockets and
focus proxy. It admits only direct-root InputOutput plugs using the screen's
default visual/depth. That constraint lets base X save-set recovery reach the
persistent root rather than a disappearing WM frame or third-party container.
It adds no mandatory library, bus service, toolkit or compositor.

Managed application focus still goes through the core command engine. A real
Escape regression exposed the core's cached same-window shortcut after focus
moved to an unmanaged proxy. Explicit Focus/FlipFocus now compare actual focus
and reuse the bounded ancestor helper to preserve application child focus. Only
a real mismatch refreshes server time through the existing FEvent helper before
reasserting focus. Automatic focus and existing policy gates remain in place.

## Executed evidence and failures

Linux ARM64 ran with native CPU virtualization (AppleHV), Ubuntu 24.04.5
container on the retained Fedora Linux lab. This is not emulated-CPU evidence
or physical-Linux qualification. The server is real fixture-owned Xorg dummy
with two active outputs; independent persistent X clients observe actual
selection ownership, window parents/maps/sizes, XEmbed messages and key events.

| Evidence | Result and scope |
| --- | --- |
| gate01 | Affected build, model, tagged module/manual install pass. Collector failed importing Meson from system Python before runtime. |
| gate02 | Canonical install provenance passes; actual Escape focus return fails. |
| gate03 | Cache repair preserves application-child focus but Escape still fails with stale core timestamp. |
| gate04 | Collector reuse assertion fails before rebuild/runtime; no product result claimed. |
| gate05 | Cache/timestamp repair passes Escape and child focus, three themes and configuration checks. Dock test fails because its partial-only strut is unsupported by this core. |
| gate06 | Supported `_NET_WM_STRUT` fixture plus optional partial metadata: all 16 actual-X case groups pass. Unchanged C binaries reused. |
| sanitizer01 | Instrumented module/model ASan, UBSan and leak checks pass; the same 16 actual-X groups pass with no sanitizer reports. Core/MFL and independent peer use normal builds. |

The 16 groups cover ownership/competing start, bounded refusal, malformed-property
recovery, maps/visibility, actual visible and hidden ConfigureRequest geometry,
keyboard proxy/traversal/Escape and child focus, 24-unit host target floor with
1-pixel client, three font/palette profiles, orientation/invalid atomic config,
primary/scale/work-area continuity, external reparent/destruction, capacity
recovery, producer disconnect, orderly restart, selection loss, and abrupt
save-set survival/redocking. Abrupt SIGKILL is a recovery check, not a successful
leak-check exit. The model and outer fixture exit results, aggregate module logs and absence of
sanitizer reports are retained; individual graceful tray exit codes were not
recorded.

The tagged installed module is matched to its exact built ELF using canonical
Meson install metadata and `fix_rpath` scratch-byte equality. Its installed man
page equals the generated man page. The fixture runs that installed module with
the current built core, MFL and independent client: it is not qualification of
a complete installed desktop. `ldd` confirms no libdbus link. The debug ELF is
1,445,824 bytes; `size` reports 352,643 text, 11,728 data and 10,840 BSS bytes.
These are feature section sizes, not RSS, idle cost or whole-desktop performance.

`make verify` passes (repository/lint and 291 portable tooling tests). Its first
run caught the README word cap; the preview was shortened and checks rerun.
The exact installed roff was rendered with local mandoc at width 78 and reviewed
for sections, configuration, controls and recovery. Rendering exits 0. Lint exits
3 on generated Asciidoctor `mso www.tmac`/`LINKSTYLE` template handling and
paragraph-spacing warnings. These remain recorded generator-portability limits;
this checkpoint does not claim a warning-free mandoc lint result.

## Scope and continuation

[Usage and recovery](../../doc/FvwmTray.adoc), the
[standalone example](../../doc/examples/FvwmTray-Standalone.fvwm), and
[capture/config manifest](../../doc/screenshots/tray-manifest.json) describe the
supported development subset. Captures are unmodified 3200×1080 framebuffers;
blue squares are synthetic clients, not tested mail/network applications.
Slate Sans, Dune Serif and enlarged Classic Mono show distinct native host
fonts/palettes; the 2× capture retains foreign 24×20 pixels.

#78 stays open for arbitrary containment/FvwmButtons/adaptive-bar integration,
other visuals and real toolkit clients, modern StatusNotifier/application menus,
persistent settings UI and complete acceptance. Global accelerators, XDND,
modality and balloons are not advertised by this subset. No protocol atom or
single fixture is a full XEmbed conformance claim. Xlib calls and fixed short
server transactions can block; work budgets cannot preempt one call.

Six-target/compiler/minimal/default-BSD-server/full-installed-desktop checks,
physical hardware/compositor and assistive-provider qualification remain at the
combined #78/R1 checkpoints. They were not rerun for this first slice. The
periodic performance counter is 1/5 after merged311; this open candidate does
not advance it. A paired core comparison is not due.

## Review and retention

Separate source review found and resolved hidden normal ConfigureRequest
allocation, traversal-fixture input continuity, and the core focus timestamp
repair. Final artifact/evidence review and repository checks are recorded with
the signed PR candidate; agent review is not human approval. Requested Temper
roles were available; actual backend model/effort metadata remains unverified.

Current source/build/sanitizer/install, required SDK/dependency caches and the
PR310 performance reference remain reusable. Current normal/failed/sanitizer
logs and captures were exported and hash-verified on the host. Duplicate guest
runs and consumed source archives were retired (112,922,942 bytes); the owned
Linux lab was stopped. The JSON receipt gives exact retained paths and current
handoff. Compact reports and published screenshots remain after raw evidence
retirement under the repository retention policy.
