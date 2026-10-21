# Standards coverage and conformance policy

Approved direction: 2026-10-06. Owner: STANDARDS-01 and the implementation owners
below. Status: **requirements and test guidance recorded; a fixed protocol
requirement inventory is complete; runtime conformance remains scoped or open**.
The tracker remains the execution authority.

## Scope and claims

Cover the core, bundled modules/helpers, default configuration, optional contrib,
installed commands/manuals and public documentation. A desktop profile is an
explicit combination of build options, OS services and enabled configuration.
Qualify the portable default separately from its optional D-Bus extension.

There is no universal desktop certification. The
[freedesktop catalogue](https://specifications.freedesktop.org/) includes stable
interoperability specifications and drafts. A spec's age is not evidence that it
is obsolete. Respect required behavior for adopted features; do not claim support
merely because an atom, library, screenshot or configuration option exists.

For every adopted requirement record: specification/version/section, component,
required or optional status, owning tracker ID, observable behavior, test/evidence,
platform/profile scope, deviations and remaining acceptance. Pin revisions in
the implementation report; changes to a living spec are reviewed, not silently
adopted. Keep draft/experimental interfaces visibly distinguished.

Use statuses **review pending**, **implemented / unqualified**, **verified for
named profiles**, **planned**, **delegated**, or **not applicable with rationale**.
Delegation requires the selected service and an integration test. Do not turn
missing required behavior into an optional exception. Any specific compatibility
break or new mandatory dependency retains its existing decision gate.

## Coverage inventory and owners

This area inventory seeds STANDARDS-01's requirement-level matrix; it does not
replace it or assert that all clauses within an area pass.

| Area | Specification / adopted contract | Current evidence boundary | Owner and required acceptance |
| --- | --- | --- | --- |
| Language and OS APIs | Project C11, reviewed POSIX interfaces and native BSD contracts | Meson selects C11; six-target builds cover existing paths, not all POSIX.1-2024. | PORT-01 / COMPAT-01: actual compiler/API feature checks, failure paths and platform fixtures; document extensions/fallbacks rather than assuming Linux APIs exist on BSD. |
| X client/WM contracts | [ICCCM 2.0](https://www.x.org/releases/current/doc/xorg-docs/icccm/icccm.html): geometry, focus, WM protocols, transients, selections and manager ownership | Substantial core implementation and scoped existing regressions; complete clause coverage pending. | EWMH-01 / COMPAT-01 / DATA-01: independent clients, correct property types/counts, timestamps, state/order and destruction/malformed-input cases. |
| Desktop window hints | [EWMH 1.5](https://specifications.freedesktop.org/wm/latest/) | Existing atom dispatch/state/work-area support; reviewed missing newer hints are candidates, not automatically mandatory defects. | EWMH-01 with MONITOR-01 / INPUT-01 / DRAW-01: supported-atom truthfulness, required hints, focus/user time, docks/partial struts, fullscreen monitors, frame extents, ping/resize-sync adoption and refusal behavior. |
| Monitor geometry/scaling | [RandR protocol, 1.5 monitor objects](https://www.x.org/releases/current/doc/randrproto/randrproto.txt); project actual-pixel wire contracts | Existing topology/scaling and scoped X tests; physical driver/toolkit acceptance remains open. | MONITOR-01 / MODULE-SCALE-01 / VERIFY-01: topology bursts, missing topology, hotplug, rotation, scale generations, primary changes, usable work areas and real GTK/Qt/browser transitions. |
| Input and rendering extensions | XKB, X input/extensions, XRender/Composite/Damage/Shape as used | Existing conditional paths; no claim to every extension or a built-in compositor. | INPUT-01 / DRAW-01 / COMPAT-01: extension presence/absence, layouts/modifiers, grabs/events, actual visuals and external compositor rendering. Negotiate features and preserve fallbacks. |
| Selection/clipboard transfer | ICCCM selections and [clipboard conventions](https://specifications.freedesktop.org/clipboard/latest/) | Bounded readers/legacy selection tests exist; complete interoperability qualification pending. | SELECTION-01 / DATA-01: PRIMARY/CLIPBOARD, TARGETS/TIMESTAMP/MULTIPLE/INCR where applicable, text encodings, chunking, owner death, late replies, cancellation and bounds. |
| Drag-and-drop | XDND for participating FVWM-owned UI; toolkit drag protocols remain client-owned | No native XDND handlers found in initial review; not a universal WM obligation. | SELECTION-01: define actual target/source roles, version/type/action negotiation, finish/cancel/timeouts and foreign-client lifetime; implement agreed desktop targets without shell/file autorun. |
| Session restoration | [XSMP/ICE](https://www.x.org/releases/current/doc/libSM/xsmp.html) | Existing session machinery and descriptor/authentication fixes; not arbitrary application restoration. | SESSION-01 / FD-01 / COMPAT-01: save/cancel/restart/logout with participating clients, authentication and descriptor cleanup, disconnect and bounded completion. |
| Desktop entries and launching | [Desktop Entry 1.5](https://specifications.freedesktop.org/desktop-entry/latest/), [startup notification](https://specifications.freedesktop.org/startup-notification/latest/) | Default launcher implements a bounded subset; POSIX shlex use is not proof of exact desktop-entry escaping. | XDG-01 / PROC-01: normative Exec parsing, visibility/localization/actions, TryExec/Path, launch context, literal argument safety and correlated startup completion/expiry. |
| Directories and autostart | [XDG Base Directory](https://specifications.freedesktop.org/basedir/latest/), [Autostart](https://specifications.freedesktop.org/autostart/latest/) | Application lookup uses XDG paths; session autostart is planned. Existing FVWM paths remain compatible. | XDG-01 / SESSION-01: override order, defaults, absolute-path requirements, data/config/state/cache/runtime separation, permissions and desktop filters. No implicit media autorun or legacy config relocation. |
| Application associations | [MIME Applications](https://specifications.freedesktop.org/mime-apps/latest/), shared MIME database and file/URI conventions | Existing xdg-open delegation; comprehensive app roles pending. | XDG-01 / CFG-01: selected applications, mimeapps precedence, filenames/URIs with special characters, unavailable handlers and safe errors. Use installed providers instead of rebuilding their databases. |
| Icons and file services | [Icon Theme](https://specifications.freedesktop.org/icon-theme/latest/), icon naming; thumbnail/trash conventions when adopted | Native image compatibility retained; default assets do not imply a full icon-theme resolver. File services may be external. | XDG-01 / IMAGE-01: size/scale/fallback and inherited icon lookup; verify selected file-manager services. Delegate trash/thumbnail handling unless explicitly implemented. |
| Shared appearance/preferences | [XSETTINGS](https://specifications.freedesktop.org/xsettings/latest/), X resources and [Settings portal](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Settings.html) where enabled | FVWM-local palettes exist; no native XSETTINGS publisher found. | SETTINGS-01 / CFG-01 / SESSION-02: single selection owner, typed bounded atomic updates, serials/restart, fonts/preferences and toolkit/browser observation. No forced scaling of client pixels. |
| Notifications/tray | [Notifications 1.3](https://specifications.freedesktop.org/notification/latest/); [XEmbed 0.5](https://specifications.freedesktop.org/xembed/latest/)/[System Tray 0.3](https://specifications.freedesktop.org/systemtray/latest/); StatusNotifier and supported menu interfaces | NOTIFY-01 / TRAY-01 planned; standard servers are not implemented by a local notice form. | NOTIFY-01 / TRAY-01: protocol/version/capability replies, real clients, malformed/flood input, expiry, actions, competing owners, appearance changes and destruction. |
| D-Bus/session services | D-Bus, login1/elogind, polkit and selected portal interfaces | Current power helper supports specific OS tools; comprehensive extension pending. | SESSION-02: borrowed/owned bus, selected service capability, denied operations, inhibitors/readiness, reconnect, missing packages and portable fallback. Never impersonate unsupported services. |
| Optional media/secrets integrations | MPRIS, Secret Service and other service protocols only when explicitly selected | External-provider integration, not new player/keyring implementations. | SESSION-02 / STANDARDS-01: inventory supported actions/providers, bounded requests and failure/owner cases. Do not add credential storage or advertise unimplemented interfaces. |
| Text and international input | Unicode UTF-8; [UAX #29 segmentation](https://www.unicode.org/reports/tr29/), [UAX #9 bidi](https://www.unicode.org/reports/tr9/), XIM/selected IME and actual font layout | Existing encoding/fonts/gettext; legacy XLookupString paths and complex-text coverage remain. | TEXT-01 / SCRIPT-01 / I18N-01: input composition, grapheme editing, shaping/bidi policy, fallback and long localized labels. Select supported Unicode data/library versions before claiming conformance. |
| Native accessibility | [WCAG2ICT](https://www.w3.org/TR/wcag2ict-22/) guidance; [AT-SPI](https://gnome.pages.gitlab.gnome.org/at-spi2-core/libatspi/) in supported optional profiles | Keyboard primitives queued; native accessible-object exports not established. | A11Y-01 / BUTTON-01/02: names/roles/states, focus/navigation, screen-reader actions/events, bounds/scaling and unavailable-assistive-service behavior. See theme guidance. |
| Theme/configuration | [Theme generation policy](THEME-STANDARDS.md), derived W3C guidance plus explicit FVWM design requirements | Existing palettes/density/motion/opacity controls; measurements and real UI acceptance remain separate. | THEME-01 / A11Y-01 / CFG-01: state colors, composited contrast, target/layout/keyboard checks, all default palettes and fallback rendering. |
| HTML documentation/public web | HTML Living Standard, applicable CSS, semantic HTML/WAI-ARIA and WCAG 2.2 AA target | Optional static generated HTML exists; no complete accessibility/conformance claim. | DOCS-01 / STANDARDS-01: validate actual exports, keyboard/reader checks, landmarks/language/link text, narrow/zoomed layouts and local offline assets; website design remains separately deferred. |
| Manuals, install and repository contracts | Existing man/AsciiDoc, Meson/install/CLI, GPL/attribution and project formatting policies | Adopted tools and scoped staged evidence; manual rewrite/release audit remain. | DOCS-01 / MAINT-02 / REPRO-01 / RELEASE-01: roff renders/lint, CLI/example accuracy, source/distribution parity, licenses and honest feature/profile claims. |

## Fixed protocol requirement register

The [2026-10-21 register](verification/STANDARDS-2026-10-21-PROTOCOL-REQUIREMENTS.md)
completes three bounded inventory stories: root capabilities/desktops, window
hints/requests and selection roles. Its forty requirement IDs refine the existing
E01–E12/S01–S08 reconciliation without reopening the twelve monitor rows. Each
records a clause or planned role, owner, source, evidence boundary and remaining
acceptance. Source fingerprints identify inspection; historical profile results
do not become qualification for current binaries. Adopt runtime repairs through
the existing owners and tracker order. Other standards areas remain queued.

## Test contract and evidence format

STANDARDS-01 owns the coverage register and CI mapping, not duplicate runtime
implementations. For each requirement use a record such as:

```text
Requirement: stable local ID + specification section/revision
Owner/component: tracker ID + source entry point
Profile: OS/architecture/X server/build options/enabled services
Status: implemented / unqualified (example, not a passing claim)
Expected behavior: externally observable protocol or UI result
Checks: independent fixture/client + positive/negative/boundary/recovery cases
Evidence: candidate fingerprint, command, log/artifact and actual result
Limits: untested hardware/toolkit, deviation or delegated provider
```

Test advertised behavior against independent protocol clients and fixtures;
do not merely assert that our implementation emits its own expected strings.
Use exact specification quoting/types/ordering and validated external data.
Exercise error paths, cancellation, ownership replacement, stale events, large
inputs and continued service after refusal. Preserve theme/module compatibility.

Pure helpers need native unit coverage; X-visible protocols need nested-X native
Linux and relevant default BSD X checks. Run feature-present/absent and optional
service profiles. Follow [the batched workflow](WORKFLOW.md): focused checks during
a coherent owner batch, extensive six-target/install qualification once at the
checkpoint, and affected checks after repairs. Native Linux sanitizer CI and
physical display/compositor/toolkit/input checks remain distinct gates.

Record a fixed set of requirement IDs at batch entry. Close that set with evidence
or explicit unresolved findings; route newly discovered defects to their owners.
Do not restart an exhaustive standards sweep after every fix or use a standards
label to authorize a broad XCB/toolkit/dependency rewrite.

Before RELEASE-01 closes, every adopted required clause needs qualifying evidence
for advertised profiles. Optional/delegated/unsupported behavior must be explicit;
open required gaps block their claim and the agreed release checkpoint. Publish
the tested profile/support subset, not a blanket compliance badge.

## Web/native distinction and references

Browsers implement [HTML](https://html.spec.whatwg.org/multipage/) and CSS; FVWM
provides window/input/display/session behavior. Apply W3C guidance to native UI
through WCAG2ICT, and test actual web exports against WCAG 2.2 AA. A palette
contrast pass does not establish accessibility or web conformance. Media-query
preferences require a supported toolkit/browser/settings bridge; FVWM colorsets
are not CSS and need no embedded browser engine.

Use the source-linked specifications above, project [development contracts](DEVELOPERS.md),
[support matrix](SUPPORT.md), [documentation policy](DOCUMENTATION.md), and
[theme guidance](THEME-STANDARDS.md). POSIX is an API reference, not an assertion
that every supported OS implements every current edition. Color-managed/HDR
output, compositor features and external application accessibility must be
qualified through their actual providers; native Wayland remains out of scope.
