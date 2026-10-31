# Optional Settings portal bridge — 2026-10-31

Owner: Cam; SETTINGS-01 #73 with CFG-01 #60 / SESSION-02 #77.
Base merged PR316 `6630bb9edc54dd305e6914ca907bcc78ae4b5f03`;
branch `codex/r1-settings-completion`. User authorized continuing both owners
through their remaining acceptance. This selects the concrete implementation of
#73's already accepted optional Settings path before implementation. #77's
session profile alone does not authorize unrelated portal implementations.

## Outcome and simpler alternative

Deliver a small read-only XSETTINGS-to-Settings backend, not a portal frontend.
An already installed xdg-desktop-portal remains the public frontend. The GTK
backend reads GSettings, so merely selecting it does not publish FVWM's existing
preferences; synchronizing dconf would add another persistent authority. Consume
the existing effective XSETTINGS publication instead. This needs no new preference,
registry row, store, private pipe, Go process or duplicate child supervisor.

Freeze six related outcomes: bounded complete XSETTINGS decoding; observed X owner
replacement/loss; read-only backend methods/signals; optional activation/packaging;
real frontend plus application effects/non-effects; source-bound focused and
relevant combined support qualification, setup/recovery and closure reconciliation.
No source/configuration migration or new mandatory runtime dependency.

## Mapping and source

One namespace/key: `org.freedesktop.appearance` / `reduced-motion`, unsigned32.
Current screen `_XSETTINGS_Sn` integer `Gtk/EnableAnimations` exactly0 maps to1;
exactly1 maps to0. Absent, malformed, ambiguous or unavailable source maps to0,
which means **no preference**, not a claim that applications enabled animations.
A source loss clears an earlier value and emits a change if needed. No-op updates
and owner replacements with the same value cause no signal churn.

FVWM's native publisher already projects the typed effective CFG `motion` value,
including overrides. An external standard XSETTINGS manager is also a legitimate
source and must be identified in evidence; the portal does not attest CFG provenance
merely from an X selection name. Font/icon/cursor settings remain on their qualified
native paths. No inferred application dark theme, contrast, accent, DPI, GTK-specific
namespace or arbitrary key export. New mappings need their own explicit semantics.

Decode complete properties of at most64KiB and at most256 records, both byte orders.
Validate every type/name/value length and four-byte alignment before access;
correctly skip integer/string/color records; reject unknown types, truncation,
trailing bytes and duplicate target keys. Target must be integer0/1. Serial is
owner-local unsigned32; rollover/replacement never uses a naive increasing test.

Subscribe to root MANAGER hints and owner Property/Structure changes, recheck
selection after subscribing and before/after reading, and discard old-owner data.
Disappearing-window errors follow existing bounded X-error conventions. MANAGER
messages alone do not prove authority. Coalesce event bursts, at most32 X/bus
messages per turn, cooperative2ms target, bounded pending output. Blocking Xlib or
libdbus calls cannot be preempted by this budget; process isolation is the boundary.

## Bus and activation contract

Native C executable `fvwm-settings-portal` accepts no arguments. Use the existing
optional libdbus dependency and local session-address policy; no bus launcher,
custom wire protocol or public frontend ownership. Dedicated non-replacing,
non-queued name `org.freedesktop.impl.portal.desktop.fvwm`, object
`/org/freedesktop/portal/desktop`, interface `org.freedesktop.impl.portal.Settings`.
Backend version1: ReadAll(as)->a{sa{sv}}, Read(ss)->v, SettingChanged(ssv),
Introspect and read-only Properties version. ReadOne belongs to the installed
frontend, not this backend. Unknown namespace/key uses portal NotFound; invalid
signatures/oversized arrays fail explicitly. Namespace patterns are empty/all,
exact or trailing section wildcard (e.g. `org.freedesktop.*`); max16 patterns,
max128 bytes each. No setting mutation method. Commit the complete latest value
before replying or signaling. Bound incoming message size/FDs/queues; a slow bus
must not grow memory without limit. Bus/X connection loss retires the process;
no internal reconnect or activation loop. Signals coalesce to the newest value.

Add explicit build option `settings-portal`, default disabled, requiring enabled
`session-dbus`. Default builds install a small refusal stub and no activation or
portal descriptor. Enabling this option opts into installing the dedicated
.service/.portal metadata; an existing wildcard/legacy desktop selection may then
choose the backend. Document that effect, do not claim metadata is inert. No new
portals.conf defaults, frontend startup, bus startup or user-file rewrite. Explicit
Settings-only selection is the supported setup and leaves unrelated configured
interfaces with their existing backend choices.

D-Bus activation requires a correct pre-established DISPLAY/XAUTHORITY environment.
Support one explicitly selected X display/screen per session bus. A borrowed bus
can span displays; never infer a caller's display or rewrite its activation
environment automatically. Missing X or explicit local bus fails without ownership.
The bus/activation owner supplies process lifetime; disabling removes selection
and retires the owned test process, never a borrowed bus or another backend.

## Verification and closure boundary

Pure decoder/mapping/filter policy tests include both byte orders, malformed
boundaries, target duplicates, all record types, absent values and serial rollover.
Real private-bus/X tests observe methods, variants, filters, signal no-churn,
competing owner, source owner changes/destruction, malformed source recovery,
slow-reader bounds, bus death and unavailable DISPLAY/address. Exercise an actual
installed frontend with installed metadata and backend: ReadAll, ReadOne, legacy
Read nesting, live updates, backend/frontend restart and explicit selection.
Compare native CFG-originated values with frontend results. Synthetic clients
qualify protocol only; actual GTK/Qt/browser effects and non-effects get separate
observations and precise profiles. Reuse existing captures when native UI is
unchanged; capture any new visible consumer sample.

Use one incremental Linux combined checkpoint, scoped ASan/UBSan for new C ownership,
enabled/disabled install/manual gates and relevant six-target native/support checks
for #73 acceptance. Reuse valid PR316 results; do not rerun its whole suite per edit.
No new comparison is due: performance1/5 after merged316,2/5 if this checkpoint merges.
The CFG registry and remaining owned image/preview acceptance retain #60; this portal
slice alone does not close it or imply whole release/hardware acceptance.

Primary sources: current Settings frontend/backend and portals.conf documentation:
https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Settings.html
https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.impl.portal.Settings.html
https://flatpak.github.io/xdg-desktop-portal/docs/portals.conf.html
Lead verified GTK backend's GSettings source and frontend1.18.4 forwarding/ReadOne:
https://raw.githubusercontent.com/flatpak/xdg-desktop-portal-gtk/main/src/settings.c
https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/1.18.4/src/settings.c
https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/1.18.4/src/portal-impl.c
Installed runtime versions/identities and actual observations remain evidence work.
