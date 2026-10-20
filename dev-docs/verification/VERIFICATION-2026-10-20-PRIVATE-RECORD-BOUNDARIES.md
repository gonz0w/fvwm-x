# Private condition, startup and monitor records — 2026-10-20

MAINT-02 #14 / CLEAN-01 #34. Base: merged [PR #127](https://github.com/gonz0w/fvwm-x/pull/127),
`b44b759dfdaa29d4e73f247152a51805a6240812`. One coherent three-story development-speed checkpoint.

## Result

Three condition/startup/monitor-adapter dependency improvements pass 149/149 affected native entries across six targets/ten profiles (15 retained focused / 134 fresh). All 5914 object pairs, ten WM executables and 249 install-output pairs preserve allocated code/data or non-ELF bytes. Ten scoped stages verify 249 identities. ARM GCC layout fanout falls 214/214/230 to 2/22/2 objects. Broader PR127 runtime evidence and required OpenBSD gaps remain source-scoped.

| Layout owner | ARM GCC previous dependencies | New dependencies / dry compile tasks |
| --- | --- | --- |
| Condition-name lists | 214 through `fvwm.h` | 2 through `condition_names.h` |
| Initial-window options | 214 through `fvwm.h` | 22 through `initial_window_options.h` |
| Monitor refresh adapters | 230 through `FScreen.h` | 2 through `fscreen_adapters.h` |

Condition matching owns the same name-list records and AND-of-ORs/inversion
semantics. Only its production and assertion-enabled core objects need the
complete layout. Initial-window option fields keep their types/order; a named
struct tag allows the existing typedef to remain opaque in common interfaces.
Startup, capture, placement, icons, session, event and update owners and their
actual-core fixture hooks include the complete layout explicitly. Monitor
refresh/allocator callbacks and their test entry point move together to a
private adapter header, consumed only by FScreen and its topology fixture.
The other FSCREEN_TESTING entry points retain their existing public declarations.

All eighteen changed C files alter include directives and blank separators only;
physical line counts are unchanged, preserving assertion-line constants. No
runtime statement, allocation, callback order, error barrier, resource lifetime,
configuration syntax, module protocol, build option or test registration changes.
All original field definitions remain exact apart from naming the initial-window
struct. Headers own complete layouts only where field access is required.

Ninja's actual full object dependency sets and default dry compilation tasks
are measured separately. Every new-layout touch restores its exact prior
mtime afterward. Fewer objects compile, while downstream links still occur;
this is a dependency improvement, not a universal wall-clock or WM latency claim.
Public API-header edits can still invalidate their API consumers.

## Combined affected qualification

All ten owner-controlled incremental profiles are qualified: Linux ARM64 GCC,
Clang, software and minimal; Linux AMD64 GCC/Clang; FreeBSD and OpenBSD on
both architectures. At most two heavy guest jobs use two Ninja jobs each.

- 149 native entries pass. Fifteen per profile cover window updates,
  style/profile/flag/command/index behavior, module packets, topology and monitor
  consumers, stack retirement and session lifecycle. The intentionally
  session-disabled minimal profile has fourteen registered affected tests.
- Linux ARM GCC's fifteen focused passes are retained only after all 18
  actual fixture/helper/WM binary hashes, the testlog and exact frozen source
  match. The other 134 entries execute fresh; Meson retains test
  prerequisites. No full runtime suite repeats merely for this PR.
- All 5914 old/current object pairs and ten production WM executables
  have equal allocated ELF sections (types, flags, sizes and content). Only
  ELF build-ID notes are excluded; debug information and full hashes may differ.
  No assertion-line exception, normalization or comparison-only recompilation
  is used. Every old object target is explicitly built, including non-default
  test objects, before comparison.
- All 249 install-output pairs preserve allocated
  code/data or exact non-ELF bytes. Ten fresh scoped stages verify
  249 installed identities after normal Meson RPATH adjustment;
  installed version and restored Ninja no-op probes pass. This scoped OpenBSD
  AMD64 stage does not qualify its broader full-install/staged-geometry gap.
- All 2499 registered names preserve per-profile ordering and
  all compile-command databases remain unchanged. Every profile retains
  75 assertion-enabled actual-core compilations; Linux retains its two
  fault-wrapper compilations. No generated defaults or build flags change.
- All 2054 frozen regular input hashes/modes pass six native
  postchecks. Baseline source proofs match all predecessor inputs and each
  baseline WM hash matches PR127's canonical qualified profile identity.
  Six canonical archives pass full gzip EOF/CRC and content/receipt validation.
- Portable `make verify` runs once: 166 passes and eleven explicit native-Meson
  capability skips. Local declaration/source review has no findings. Compiler
  diagnostics remain in the receipt; no new layout header warning occurs.

One qualification harness assertion requested `session-xvfb` in the minimal
profile before checking its registration. Session support is intentionally
disabled there. No native test failed. The first result/archive/log remain;
a minimal-only continuation finishes the fourteen registered tests, comparisons,
install and dependency probes, retaining every earlier successful command.
The frozen production source and existing test registrations are unchanged.
The original runner and both source-bound continuation scripts are archived.

The initial capacity preflight refused before builds at 681,492,480 free bytes.
After full canonical-host gzip/CRC and individual-file verification, only
completed task-owned guest duplicate captures were removed (624,073,270 bytes).
Canonical host evidence, sources, builds, stages and VM disks remain preserved;
free space was 1,308,872,704 bytes. The 1 GiB owner / 256 MiB temporary floors
and job limits remain unchanged.

## Evidence and retained scope

Signed author-DCO implementation: `1becb2034d0a219cfcd8b416a16097f460cb22b8`;
tree `e5727625617c1c6b660171f733779eff4fbe4209`. Frozen archive SHA256:
`6e8b599ab50aa011621922a123191733fa706cbfe8ce943a18dd880ef85809ee` (2054 regular files).
All archive bytes/modes bind to that Git tree. Final source changes after the
checkpoint are Markdown and the machine receipt only.

[Machine receipt](VERIFICATION-2026-10-20-PRIVATE-RECORD-BOUNDARIES-COMBINED.json) binds source, scripts,
profile tests, dependencies, object/install comparisons, canonical archives,
portable verification and local review. Raw evidence is retained under
`builddir/codex-2026-10-20/private-record-boundaries` in the owning workspace.

Exact allocated-code/data parity retains [PR127's broader runtime/default-X/
sanitizer evidence](VERIFICATION-2026-10-20-STYLE-FLAG-MUTATIONS.md) in its
original source/profile scope. No fresh full runtime/default-server/sanitizer
execution is claimed for this declaration/include-only checkpoint.
Three required OpenBSD AMD64 omissions remain: CI asset-worker timeout,
SVG/MFL startup refusal and PR120 legacy-property first-connection refusal.
The original unchanged legacy-property control and unproved cause stay retained;
there are no new retries or deadline changes. Its broader full install and two
staged geometry probes remain unqualified.

Linux is macOS-hosted; BSD native kernels run under QEMU. Native-CPU Linux
sanitizers, physical GPU/input/mixed-monitor hardware, actual toolkit/compositor
and release acceptance remain separate. The user-closed review QEMU/VNC stays
closed. Actions is disabled; VESA #93 remains deferred. Broader MAINT-02,
CLEAN-01, STATE-01 and STYLE-01 scopes stay open.
