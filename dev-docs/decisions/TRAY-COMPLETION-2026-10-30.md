# TRAY-01 completion contract — 2026-10-30

Owner: Cam. Selected by the user's direction to merge PR313 and finish #78 and
related work. Base `db6b51ad29c515e06e0d2e0918eead2fc562d240`;
`codex/r1-tray-completion`. This freezes the remaining implementation before
coding. Existing configuration syntax and root-popup defaults remain usable.
This is a specification, not passing evidence or a dependency-default change.

## Modern protocol and shared consumers

Extend FvwmTray with an opt-in adapter under the existing `session-dbus` feature.
The disabled build retains XEmbed without linking libdbus. `StatusNotifier Off`
is the native default. Shared typed settings add `tray_modern` (false) and
`tray_icon_size` (8..128 logical units, default24), migrating schema1..7 to8.
Only FVWM-owned modern icons scale; foreign XEmbed clients remain actual pixels.
Expose native `IconSize` and per-component `IconTheme` (hicolor default) without a
second theme resolver/store. Existing four tray settings remain unchanged.

Support both published org.freedesktop.StatusNotifier and practical
org.kde.StatusNotifier dialects, including signed/unsigned WindowId without using
that property as authority for a managed-window action. Use only the inherited
explicit local Unix session-bus address. Never autolaunch, activate a service,
create a bus or replace/queue for an existing watcher. Own absent watcher names;
register as a host and discover items when a watcher already exists. Bind each
watcher/item/menu to actual unique owner, path and generation, deduplicate dual
registrations, track owner disappearance, and reseed after watcher changes.
Bus disconnection retires modern state while XEmbed survives; explicit
reload/re-enable retries only the same inherited bus contract.

Implement watcher registration methods, registry/host/version properties and
signals. Validate registration-by-name against the registering sender's actual
owner; object-path registration belongs to that sender. Read Category, Id,
Title, Status, ItemIsMenu, Menu, IconThemePath, normal/attention/overlay names and
pixmaps, WindowId and ToolTip. Coalesce property/icon/status/menu signals and
PropertiesChanged. Passive entries retain identity but leave visible layout;
attention selects attention with normal/text fallback. Support Activate,
SecondaryActivate, ContextMenu and Scroll using actual root coordinates.
Tooltips are bounded plain text, without animated movies or rich markup.

Resident bus calls are asynchronous: a fixed outgoing-serial table records
expected owner, operation, item generation and deadline; late/stale replies
cannot mutate reused slots. Do not mix pop_message dispatch with pending-call
callbacks. Reuse the existing isolated five-second bus-startup guard, extracted
to a common helper while preserving the notification wrapper's contract.

| Resource | Bound |
| --- | --- |
| Combined retained XEmbed/modern entries |32 |
| Watcher item records / registered hosts |32 /16 |
| Outstanding bus requests |8; one property refresh per item |
| Request timeout |2s |
| Service work |32 messages/turn, cooperative2ms target |
| Incoming message / receive threshold / outgoing bytes |1MiB /2MiB /64KiB |
| Pixmap variants / dimensions |8 per property /1..256; exact checked width×height×4 |
| Retained icon bytes, including replacement |4MiB global |
| Active menu |one;128 nodes, depth4,64KiB parsed text/data |
| Icon acquisition |one child,8 requests/batch,4s deadline |
| Icon source/publication/decoded source pixels |8MiB /2MiB /256×256 |

Queue thresholds do not establish hard RSS or preempt one library/X call. Bound
walks, outputs and retained replacements; explicit refusal/unavailability rather
than silent or unbounded retries. Modern cells are native-owned and never enter
foreign save sets or foreign XEmbed protocol operations.

Use the existing fvwm-desktop-worker in private `--tray-icons` mode with fixed
absolute executable/argv, nonblocking framed/hex pipes, bounded child lifetime
and generation-tagged results. Preserve its no-argument JSON protocol. Extend
existing xdgsource/iconrender with smaller consumer budgets and bounded extra
roots for IconThemePath; reuse the same finder and immutable acquisition.
Validate decoded geometry before compressed-image decoding. Native picture
leases retain their source generation until upload/loading completes; retire
owned superseded temporary files afterward. Bus pixmaps decode network-order
ARGB and use existing picture_argb upload. Missing worker retains pixmap/text
fallback and reports missing named-icon availability.

One native menu popup shares tray font/colors/scale. Support normal/separator,
enabled/visible/check/radio state, bounded nested submenus, labels and shared
named icons. AboutToShow precedes bounded GetLayout with explicit properties;
Event(clicked) carries only current numeric ID/layout/owner generation.
LayoutUpdated/ItemsPropertiesUpdated invalidate stale activation and coalesce a
refresh. Pointer, arrow/Enter/Escape/submenu-return and focus restoration work.
Unavailable menu falls back to item ContextMenu. Hide, owner loss, disable and
replacement close it. Application labels never become FVWM/shell commands.
Menu icon-data, rich content and animation are outside this supported subset;
this is item menus, not a global application-menu registrar.

## Optional native Buttons container

Add one explicit `Tray` button option per Buttons instance, separate from generic
Swallow. The default desktop remains the root-popup launcher. The slot discovers
the current screen's tray selection owner and existing _FVWM_TRAY_HOST, requiring
a versioned native container capability and XFixes server>=1 on both sides.
Use the already optional xfixes dependency, linked only to affected targets when
found. Missing build/server support refuses containment and preserves root mode.

The slot owns allocation; Tray never resizes its parent. Fixed X properties and
ClientMessage fields negotiate one offer/attachment: slot XID, a fresh dedicated
offer-window XID, host XID, selection-owner XID and protocol version. The offer
window/property binds attempt identity; validate screen, ancestry, dimensions,
current ownership and exact host/capability before mutation. One pending/attached
container, bounded2s offer lifetime; a second container cannot displace it.
Buttons installs a root-target/unmap XFixes save-set entry for the foreign host
before offering. Tray installs equivalent entries for every foreign icon before
docking. Tray changes allocation mode before reparenting and acknowledges the
exact offer. Destroyed/stale offers and partial X failures roll back once.

Normal detach unmaps/reparents host to root, confirms release, then removes
Buttons' protection before destroying slot ancestors. If confirmation fails,
retain protection and let connection teardown recover; do not explicitly destroy
that ancestor. Buttons death leaves the host rooted/hidden. Tray death leaves
foreign icons rooted/unmapped, including both termination orders. Connection
save sets do not protect arbitrary third-party parent destruction while its
owner remains alive; do not advertise that guarantee.

Attached layout uses slot-local pixels and translated slot monitor scale.
Compact mode omits the above-icons header, reuses bounded paging and reserves
24-logical-unit side controls where they fit, with keyboard fallback for smaller
allocations. Empty/disabled/undersized slots have deterministic clipped text and
input. No parent/child resize feedback loop. Attached Show/Hide controls slot
content; ToggleAt refuses moving/detaching an attached host.

Tray slots participate in Buttons keyboard navigation. Managed Buttons focus
still goes through the core command engine before proxy delegation. Escape and
outward traversal return to surrounding controls; external application focus is
restored only on leaving the container focus scope. Handle ancestor unmap,
disable and deactivation. Preserve SlateBar's current focus style; ship a
focus-capable opt-in container example. Owner loss clears attachment; reconnect
only to a newly validated native owner.

## Completion and related issue boundary

Implement a coherent set before one focused Linux/bus/X gate. Pure/protocol tests
cover all bounds, ownership/generation/expiry, pixmap byte order, menu changes,
icon framing/budgets and attachment/layout. Actual bus/X exercises both dialects,
incumbent watcher hosting, multiple paths, producer loss/restart/late replies,
menu mutation/input/focus, scale/theme changes, no-bus survival of XEmbed,
container rejection/detach/resize/pages/rebuild and both crash orders. Reuse
unchanged accepted cases; add regressions for actual changes.

Before #78 closure, qualify representative real SNI/dbusmenu and XEmbed clients
with recorded toolkit/version/profile, and the combined affected Linux/FreeBSD/
OpenBSD AMD64/ARM64 enabled/disabled builds, native tests, installs/manuals and
relevant X server checks. Inventory existing lab clients first; no implicit
package installation. Inspect three material theme/font configurations. Measure
resident/startup/idle/dependency costs. Record unavailable coverage honestly;
no synthetic-only or feature-disabled run counts as real-client qualification.

#77 shared bus consumer checks can compose with this work; unrelated session,
portal, locker and generic icon/theme owners stay independent. PR313's full
Buttons font-lifetime leak gap stays recorded under its finite R2 follow-up,
without a fabricated baseline or clean full-session claim. #257/#258 stay parked.
A new PR remains open for maintainer review. Close only criteria with linked
results; no unrelated upgrade is added by inference.

Primary protocol references: [watcher](https://specifications.freedesktop.org/status-notifier-item/latest/status-notifier-watcher.html),
[host](https://specifications.freedesktop.org/status-notifier-item/latest/status-notifier-host.html),
[pixmaps](https://specifications.freedesktop.org/status-notifier-item/latest/icon-pixmap.html),
[KDE item XML](https://raw.githubusercontent.com/KDE/kstatusnotifieritem/master/src/org.kde.StatusNotifierItem.xml),
[dbusmenu XML](https://raw.githubusercontent.com/Alexays/Waybar/master/protocol/dbus-menu.xml),
[XFixes](https://xorg.freedesktop.org/archive/current/doc/fixesproto/fixesproto.txt).

## Focus integration discoveries

The accepted surrounding-control keyboard contract requires the topmost
Buttons embedder to own a leaf focus proxy when its native Tray slot and
keyboard navigation are enabled. Focusing the containing MyWindow alone lets
X route keys to a foreign child beneath the pointer. Follow the
[XEmbed keyboard rationale](https://specifications.freedesktop.org/xembed/0.5/rationale.html#keyboard-focus):
keep the proxy outside the slot/host/client subtree and route the module's own
navigation through it. Ordinary Buttons instances retain their existing path.
Managed entry still uses the core command engine. Delegation must atomically
check actual current scope; a late activation cannot take focus from another
application. Internal return accepts literal MyWindow or the current Tray host
subtree, refusing sibling application children; navigation/redraw follows the
focus transaction. A proxy is an internal widget, not another managed window.

Actual integration also exposed two separate return defects. Unmanaged focus
notices must not replace Buttons' cached external managed target; consume the
existing frame word and require two focus-packet words in this consumer. Core
explicit Focus/FlipFocus needs fresh server time for a real different-target
transition after module-owned CurrentTime focus, while preserving the
same-managed-window child-focus no-op and existing refusal/NoWarp policy.
[XSetInputFocus](https://xorg.freedesktop.org/archive/X11R7.5/doc/man/man3/XSetInputFocus.3.html)
ignores requests older than the last focus change. The correction adds one
existing synchronous server-time roundtrip to affected explicit transitions;
pointer focus and idle dispatch do not acquire that barrier. These are bounded
implementation requirements of the selected focus acceptance, not configuration
syntax changes or passing evidence. Retain the failed runs and actual request,
child-focus, refusal and pointer-recipient observations.
