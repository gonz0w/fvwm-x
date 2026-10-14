# FVWM-X project

FVWM-X modernizes FVWM3 into a responsive, dependable X11 desktop on Linux,
FreeBSD and OpenBSD. Keep FVWM's efficient single-threaded window-manager core,
process-isolated modules and compatibility with existing configuration files.

Current work, status and execution order belong to
[PROJECT-TRACKER.md](PROJECT-TRACKER.md). This document records scope and the
requirements migration; it is not another backlog.

## Product constraints

- The first release retains the full approved modernization/hardening/performance
  and desktop-feature scope. Follow the tracker order: refine the foundation
  before adding features, then qualify the release. A smaller release milestone
  was not adopted; bounded checkpoints prevent drift without reducing scope.
- X11/XRandR 1.5+; no native Wayland backend or built-in compositor.
- Authoritative window/X state stays in the core's single thread. Bound queues,
  event-loop work and interaction servicing; preserve dispatch/command order.
- Scale FVWM-owned UI and geometry, not arbitrary application pixels. EWMH,
  ICCCM, configure and module geometry remain actual X11 pixels.
- Compatibility is limited to existing FVWM configuration files and their
  format. Keep supported names, arguments, bindings, functions and theme
  directives usable without a configuration rewrite. Ask only for a deliberate
  configuration incompatibility or required migration. Internal APIs/ABIs,
  module protocols, exact legacy rendering, incidental timing and failure
  behavior have no separate compatibility hold; update shipped consumers,
  documentation and verification together. Other product constraints remain.
  See the [clarified compatibility scope](decisions/CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
- Approved runtime language direction: retain C core/native primitives, replace
  first-party Python runtime helpers and portable Perl utilities with Go, and
  add optional isolated Go-hosted Lua policies over shared primitives. The user
  explicitly approves future retirement of FvwmPerl/perllib and Perl-specific
  APIs; unrelated FVWM/FvwmScript contracts remain. Runtime ports/removal and
  go-lua adoption are queued; first-party build/test tooling follows separately.
  See the [component audit and scope](decisions/RUNTIME-GO-LUA-2026-10-06.md).
- The default desktop uses existing FVWM components and base-system capabilities;
  unavailable services are reported honestly. No additional desktop framework
  or mandatory GUI toolkit is introduced by documentation or cleanup work.
- Shipped geometric artwork uses self-contained SVG icons, wallpapers, palette
  previews and branding. The complete visual profile uses the existing isolated
  SVG renderer; minimal builds retain labeled controls and plain backgrounds.
  PNG remains appropriate for screenshots and existing raster-image support.
  See the [artwork decision](decisions/THEME-SVG-ARTWORK-2026-10-06.md).
- Approved session direction: a capable portable session manager in the default
  install, with comprehensive optional D-Bus integration extending the same
  settings/actions/UI. Missing optional services must not prevent login. Runtime
  work remains queued; see the [session scope](decisions/SESSION-INTEGRATION-2026-10-06.md).
- Approved file-manager direction: reusable native collection, directory and
  action primitives first, then reliable file-operation jobs, optional desktop
  pinboard and application interoperability. A separate config/theme build
  thread can consume each qualified primitive checkpoint while core work
  continues. Advanced features remain individually deferred candidates; no
  mandatory GUI toolkit or new Open/Save portal backend is implied. See the
  [file-manager scope](decisions/FILE-MANAGER-PRIMITIVES-2026-10-06.md).
- Approved clipboard direction: reusable typed content, selection transfer,
  retention and protected-storage primitives first, then a text/image consumer
  handoff for a separate configuration/theme thread while runtime work continues.
  Reuse the file-manager collection/action contracts. Protected memory precedes
  optional encrypted persistence; password retention and defaults remain open.
  X11 display clients remain outside the confidentiality guarantee. See the
  [clipboard scope](decisions/CLIPBOARD-PRIMITIVES-2026-10-06.md).
- Approved desktop exploration direction: additional consumers and hardware,
  application and richer-layout capabilities wait until the existing program is
  complete. Shared capability/state, typed-action, asynchronous-operation,
  collection-filtering and owned-setting preview/revert contracts are scoped
  into current CFG/IPC/SESSION/FILE work, using existing consumers. Future
  features add no release prerequisite or backend/dependency adoption. See the
  [shared scope and inventory](decisions/DESKTOP-EXPLORATION-2026-10-06.md).
- Requested late localization polish covers all eleven current catalog languages
  across first-party UI, manuals and shipped configuration comments after the
  existing program. Preserve executable configuration and user files, qualify
  language rendering/input, and keep translations usable offline. Tooling and
  service choices remain proposed. See the
  [localization scope](decisions/LOCALIZATION-POLISH-2026-10-08.md).
- Follow the six-target [support matrix](SUPPORT.md). Pure policy needs native
  tests; X-visible changes need real-X integration. Native Linux sanitizer execution
  and physical hardware/compositor qualification remain separate evidence gates,
  performed locally. GitHub hosts code/issues/PR review with Actions disabled.
- Adopt and qualify the applicable [standards contracts](STANDARDS.md) for the
  first release, with requirement-level evidence and honest profile-specific
  claims. Native defaults follow [WCAG2ICT-adapted theme guidance](THEME-STANDARDS.md);
  actual web exports target WCAG 2.2 AA. This does not mandate a new toolkit,
  browser engine, GTK/Qt theme creation or every optional desktop service.

## Monitor and HiDPI requirements migration

The original milestone files now live in
[history/planning](history/planning/README.md). Preserve their requirements,
decisions and verification history, but do not use their old checkboxes as
current passing evidence. Some checkboxes predate later repairs, and the old
requirements contain a compatibility policy superseded by the user.

Historical identifiers are written here with a `v0.1/` prefix. For example,
`v0.1/GEO-01` means monitor ownership, while the active tracker uses `GEO-01`
for the completed configure/coordinate fix. Historical `COMPAT-01/02` also
collide with current compatibility/Perl entries. Never import those IDs directly.

| Historical requirements | Current owner and qualification |
| --- | --- |
| v0.1/TOP-01, TOP-02, TOP-03, TOP-06 | Committed topology, ownership, burst recovery and live-X harness. Preserve existing monitor policy/tests; ongoing qualification belongs to MONITOR-01, COMPAT-01 and VERIFY-01. |
| v0.1/TOP-04, TOP-05, SCALE-06 | Monitor removal/recovery, inspection/simulation and selective scale reload. MONITOR-01 first reconciles current implementation and evidence before identifying missing behavior. |
| v0.1/SCALE-01 through SCALE-05 | Rational overrides, automatic DPI, precedence, diagnostics and supported quarter steps. Existing scale/config policy remains; MONITOR-01 verifies reload/transition dependencies. |
| v0.1/UI-01 through UI-06 | WM chrome/fonts/images/cursors/interaction scaling and resource keys. COMPAT-01 and IMAGE-01/DRAW-01/CACHE-01 retain the relevant contracts; MODULE-SCALE-01 audits bundled UI consumers. |
| v0.1/GEO-01 through GEO-07 | Ownership, logical size, ICCCM constraints, drag anchor, work areas, restore/fullscreen and physical-pixel protocol. Existing geometry/interaction tests are retained; MONITOR-01 reconciles hotplug/reload, with INPUT-01 for lifetime/interaction changes. |
| v0.1/MOD-01 through MOD-03 | Additive monitor/scale metadata and bundled module consumers. MODULE-SCALE-01 audits coverage before extending contracts, coordinated with IPC-02 and COMPAT-01. |
| v0.1/TEST-01 through TEST-05 | Arithmetic/geometry policies, nested-X topology and client/compositor/build profiles. VERIFY-01/COMPAT-01 and SUPPORT.md own qualification; no archived claim closes remaining physical-GPU/toolkit/native-CI gaps. |

Future ideas from the archived milestone remain candidates: per-application
pixel preservation, scale percentages outside the supported buckets,
reconnected-monitor placement and richer recovery history. They are not
approved compatibility changes or implicit additions to the current build.
Evaluate them against concrete needs under MONITOR-01/CFG/IPC before creating
new runtime work.

## Documentation ownership

Developer guides explain how to change and verify the product. User-facing
commands, options, services and examples belong in `doc/`, generating installed
man pages and optional HTML from one source. The documentation
[policy](DOCUMENTATION.md) and [workflow](WORKFLOW.md) define upkeep.
