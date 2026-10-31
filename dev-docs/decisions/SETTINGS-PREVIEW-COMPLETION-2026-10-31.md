# CFG-01 owned background preview completion — 2026-10-31

Owner: Cam; #60 CFG-01 / #70 IPC-02. Base merged PR316
`6630bb9edc54dd305e6914ca907bcc78ae4b5f03`; branch
`codex/r1-settings-completion`. User explicitly requested continued #60/#73 work
toward closure. This freezes the remaining current settings/image-preview slice
before implementation. Existing component evidence remains source-bound.

## Finite registry and preview boundary

CFG-01 closure covers the current26 typed preferences and their existing owners:
palette, appearance, day, night, transparency, motion, keep_wallpaper,
launcher_opacity, wallpaper_palette, wallpaper_appearance, contrast, session_bus,
session_notifications, notification_dnd, session_power, tray_enabled, tray_gap,
tray_padding, tray_orientation, tray_modern, tray_icon_size,
native_settings_enabled, application_font, icon_theme, cursor_theme, cursor_size.
Retain type/default/range/source/capability/override metadata, private atomic
persistence, scoped reset, old-schema read and saved/effective distinctions.
Future feature-owned registrations have their own finite contracts; they do not
turn CFG-01 into unbounded registration/preview work.

The supported reversible preview set is eight existing keys: palette, appearance,
contrast, transparency, keep_wallpaper, wallpaper_palette, wallpaper_appearance,
and launcher_opacity. Include both contrast directions, transparent palette and
appearance, and appearance=auto as a captured current schedule phase. One preview
changes one key and retains unrelated drafts/overlays. A choice with no changed
visible projection is refused before mutation with an explicit reason; ordinary
save remains available (e.g. retaining an already-current wallpaper).

Day/night scheduling, motion, application preferences and tray layout remain typed
save/apply owners with truthful preview capability/reasons. Service/lifecycle
settings do not gain speculative rollback. Their ordinary independent
font/icon/layout/application behavior must be linked to actual owner evidence at
closure. No arbitrary command, display adapter, font engine or GTK theme rollback.

## Eight completion outcomes

1. Add minimal core-owned background publication and a Backer selector adapter.
2. Add bounded private solid/PNG aspect-cover/tint preparation using the existing
   image manager/isolated worker, with retained input through actual worker reap.
3. Extend the existing resource lease with a finite version3 complete bundle.
4. Publish/restore exact retained root/role/menu resources at the outer boundary.
5. Confirm runtime before scoped conditional persistence; retain typed overlays.
6. Preserve phase/draft/freshness and defer normal theme replay until recovery.
7. Expose useful per-key preview capability, states and refusal/recovery UI.
8. Qualify combined normal/scoped sanitizer/actual-X/theme/support/install/manual
   results and reconcile every #60/#73 criterion with its actual evidence.

## Core-owned root prerequisite

Add Backer option `CoreOwnedPixmap`, default false for existing configurations.
Only its Colorset20 path delegates a fixed internal `Send_Root 1 20` selector to
core module dispatch. Other colorsets, solids, commands, RetainPixmap,
DoNotRetainPixmap and all legacy root functions retain their existing behavior.
The shipped default selects CoreOwnedPixmap instead of RetainPixmap. Its background
lifetime ends with the WM X connection; Backer exit does not release the core root.
Document the new mode and unmodified legacy mode. No configuration migration.

Core owns one current immutable root picture/color, dimensions/source revision,
one bounded pending preparation and expected publication notifications. Keep
selector command dispatch nonblocking; coalesce identical requests. Use existing
picture/color retention. A lease retains original/candidate root references, never
a Backer pointer. Refuse raw/unowned root adoption for preview.

NEVER put a core-owned XID in ESETROOT_PMAP_ID or _XSETROOT_ID: legacy root utilities
can retire the client identified there. Publish only _XROOTPMAP_ID. Explicit
initial takeover can remove predecessor kill-marker properties, without killing
or reclaiming their clients. Preserve external changes and release only proven
owned references. Shutdown removes only its still-matching _XROOTPMAP_ID before
closing X. Parked #257/#258 pathname/security/Backer optimization work remains
parked; this adapter is strictly necessary preview ownership and does not alter
legacy root-client reclamation or pathname policy.

## Private rendering and finite plan

Shipped backgrounds are PNG; their existing ordinary loader and legacy Backer
aspect scaling are synchronous. Do not move that work into the core dispatch loop
and call it asynchronous. Add a bounded private background job to the existing
image manager/worker, reusing/extracting existing PNG decoding and checked image
limits. It opens/reads the bounded literal source path in the isolated process and
returns the existing bounded ARGB/result/conversion/X-ack representation. Support
solid, selected PNG aspect-cover, and tinting an already-owned prepared root for
colorset10. Refuse unsupported formats/private ownership before publication.
Ordinary loading/path configuration remains unchanged. No new worker pool/resident.

The existing Render fast path remains available for tinting. Its finite Xlib
queries/acknowledgements are not a claim of round-trip-free core execution;
software fallback uses the existing isolated effect worker. Render's premultiplied
ARGB8888 quantization can differ by one channel unit from the RGB16 software
blend. Verify each profile against its own exact independent reference.

Private candidates own fourteen roles, optional colorset20/root, buffered
colorset10 and two menu scalars. Bind image completion to private slots; callbacks
settle those slots and wake outer work, never ordinary colorset publication.
Preserve existing outer order: image completion, resource publication, window flush.

Version3 native token:

```
3:<336 role hex>:<2 flag hex>:<3 menu opacity>:<3 launcher opacity>:<6 RGB>:<base64 path>
```

Flags: bit0 enables buffered root transparency; bits1–2 select root mode
0=retain,1=solid,2=PNG (3 invalid); bit3 preserves captured actual menu scalars.
All other bits are invalid. Opacities are decimal000..100. RGB is six lowercase
hex digits. PNG requires strict canonical padded base64 of1..256 decoded pathname
bytes, no NUL; other modes require `-`. No decoded command parsing or raw public
Pixmap ID. Reject bad combinations before allocation. Preserve-menu uses the
actual captured independent menu/launcher values (palette/appearance/wallpaper);
transparency/contrast intentionally apply resolved scalars. Versions1/2 remain.

Maximum native token701 bytes; full Send_Preview command880 bytes (881 with
newline/NUL), retaining the1000-byte bound. Public JSON retains4096-byte requests
and8192-byte replies. Update native C/MFL/Go producers, capability and manuals
consistently. Parser/boundary tests establish the actual calculated limits.

## Publication, recovery and persistence

Keep one lease,64KiB metadata cap, five-second preparation and existing15-second
(default)/30-second(maximum) active lease. Existing decoded/pixmap limits also
bound actual pixels. Validate ownership, root dimensions/topology, appearance
generation, colorsets and menu identities before apply. Move the complete bundle
at one outer boundary, submit root publication, then broadcast. Do not claim
simultaneous repaint across processes.

Attribute exactly the expected own root notification by request serial/ordinal;
never suppress all root invalidation or match solely by current value. Update
owned root bookkeeping directly, without reparsing transaction-owned colorset10.
Refuse root-changing transactions with unsupported dependent colorsets outside
the bundle. A repeated Backer selector after colorset20 broadcast is a no-op for
the published source identity and cannot replace a lease or extend its deadline.

Preparation failure/cancel/timeout leaves the published originals intact.
Cancel/expiry/controller loss after apply restores exact retained resources
without reopening source files. Backer loss leaves core ownership valid.
External root/Colorset/MenuStyle changes preserve external state and return
conflict; topology changes invalidate stale-size preparation/publication.
Confirmation keeps the candidate and releases originals. Only then save the
selected key using captured file revision. Save failure reports runtime-kept /
preference-unsaved and retains the bounded typed effective overlay.

Replace the three-string runtime overlay with a typed allowlist for the eight
keys; preserve earlier unsaved overlays and unrelated editor values. Update
lastTheme/lastWallpaper/effective wallpaper caches to confirmed projection without
replaying theme commands. Restart inherits no lease or runtime overlay. Recheck
current settings/file/core generations and resolved schedule phase at apply and
confirm. Phase changes recover the active lease before normal theme replay; an
unconfirmed recovery cannot be treated as permission to replay over unknown state.

## Modal publication correction

Native source review after the modal recovery gate found two integration defects:
preview publication erased the root fence's retry-versus-conflict distinction,
and the fence read ConfigureNotify's receiving window instead of its changed
window. Correct both within this checkpoint. Propagate an operation-local
ready/retry/conflict result through apply, confirm and restore. Retry must release
the grab/staging, preserve the exact pending lease/bundle/deadline, and return
before mutation so ordinary dispatch can drain bounded queued events. Preserve
failure for genuine root/topology/ownership/menu conflicts, the64-event fence
and original event ordering. Test queued unrelated property events and child
ConfigureNotify separately from genuine root/topology changes; do not turn a
failed reproduction into passing evidence. Gate18's precise trigger remains to
be classified from its failure-only diagnostics.

## Acceptance and stop condition

Pure native/Go tests cover plan/flag/path/arithmetic bounds, state/ownership,
typed overlays, draft/file/phase races and capability reasons. Actual-X must
observe root pixels/property/roles/buffered backing/two scalars independently,
including deleted source before restore, both transparency/contrast directions,
followed/independent/retained wallpapers, minimal/plain background, failed decode/
allocation, expiry, nested menu/Wait, controller/Backer loss, external root and
topology changes. Reuse scoped ASan/UBSan and three material theme workflows.
Run affected support/default-X/image/backend checks at the combined checkpoint,
not per edit. Failures/skips remain distinct; global release/hardware acceptance
retains its own owner. Close each issue only against its frozen criteria, with
explicit per-profile results and resolved findings.
