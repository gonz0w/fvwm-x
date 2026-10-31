# Native application preferences checkpoint — 2026-10-31

Owner: Cam; CFG-01 #60 and SETTINGS-01 #73. Base merged PR315
`b19c3b04d5d423bdd3ba05ec266c820389304de0`; branch
`codex/r1-native-preferences`. User authorized continued coherent feature work.
This freezes the selected slice before implementation, not passing evidence.

## Outcome and boundary

Deliver seven related outcomes: central typed application preferences and schema9;
compact native editor; opt-in observed helper lifecycle; bounded complete
XSETTINGS projection; freshness/no-churn/restart handling; ownership-preserving
Xcursor resource overlay; independent native/GTK observations and documentation.
The existing CFG store remains authoritative. A fixed installed native C child
`fvwm-native-settings` is owned by FvwmDesktop, like the accepted isolated native
bus bridge. It is simpler than a new persistent module with core alias/rebind
arbitration. No core/MFL protocol or managed-window authority changes.

| CFG preference | Type/default | Published mapping |
| --- | --- | --- |
| native_settings_enabled | boolean/false | Optional helper lifecycle only |
| application_font | UTF-8 string/empty | Gtk/FontName string |
| icon_theme | theme name/empty | Net/IconThemeName string |
| cursor_theme | theme name/empty | Gtk/CursorThemeName string; Xcursor.theme |
| cursor_size | integer0..128/0 | Gtk/CursorThemeSize integer; Xcursor.size |
| motion (existing) | boolean/current default | Gtk/EnableAnimations integer0/1 |

Empty strings/zero mean omit the owned value and inherit toolkit/environment
fallback, not force a font/theme. Fonts: at most128 UTF-8 bytes, no C0/C1 controls,
NUL or line separators; Go may reject other nonprintable Unicode. Theme names:
1..128 ASCII letters/digits/underscore/hyphen/dot, excluding dot/dot-dot, or empty.
These are data, not paths or commands. Registry checks syntax, not installed
font/theme existence. The new application group is separate from the existing
appearance editor; old schemas load with off/inherit defaults without writing.
Existing explicit configuration overrides retain precedence. Ordinary successful
save uses the existing private atomic file and upgrades schema, with no migration
requiring user configuration edits.

Do not derive GTK themes/dark preference from FVWM palettes/appearance or publish
DPI from monitor scale. Portal settings, theme creation/installation, arbitrary
client pixel scaling, fonts/icons inside FVWM's own rendering, automatic package
installation and blanket toolkit/browser compliance are separate scope. Whole
#60/#73 closure retains its frozen remaining criteria.

## Private pipe contract

ASCII lines, maximum2048 bytes including newline; exact field count:

```
FNS1 APPLY desktop32 run32 seq motion cursorSize fontHexOrDash iconHexOrDash cursorHexOrDash
FNS1 RESULT desktop32 run32 seq xstate resourceState xserial reason
```

`desktop32` is the existing Desktop instance; `run32` is fresh lowercase hex from
Go newID(16) on every launch. Decimal unsigned64 sequence starts at1. Motion0/1;
cursor0..128; strings are lowercase hex of at most128 decoded UTF-8 bytes, `-`
means empty. Native result xstate=ready/conflict/lost/failed/off;
resourceState=applied/inherited/conflict/unavailable; unsigned32 actual X serial,
zero before publication. Reasons are fixed tokens, including none, owner-conflict,
invalid-input, stale-input, x-error, resource-conflict, resource-limit, parent-eof.
No external strings in diagnostics. Partial frames are preserved; incomplete EOF,
overflow, extra tokens and malformed encodings never become actionable tuples.

C binds permanently to the first desktop/run pair. Updates require that pair and
increasing sequence. Exact duplicate sequence/payload can return the existing
result without X writes; same sequence/different payload or older sequences are
refused without mutation. Publication serial/per-key last-change serial are
independent of request sequence, with unsigned32 rollover and no unchanged-value
churn. One complete snapshot is validated before publication; no partial fields.
Tokens provide freshness, not hostile-client authentication.

## Ownership and bounded work

Desktop owns one supervisor, one child slot, one in-flight apply and one latest
unsent desired tuple. Readers/writers use bounded buffers/channels and immutable
run-tagged events; only the controller loop mutates controller state. Drain at
most16 helper events per existing tick, retaining terminal completion separately.
No per-update goroutine, queue growth or blocking helper I/O in the controller.
Use the fixed installed helper path plus an explicit absolute development override
`FVWM_NATIVE_SETTINGS_HELPER`. Enabling is an observed attempt: Start is not ready.

The C child uses the current X display/screen, existing bounded property and X
error conventions, at most32 input/X events per turn and a cooperative2ms target.
A blocking Xlib call cannot be preempted by that budget. Validate the initial tuple
before acquisition. For at most2seconds, recheck an existing selection owner with
fresh server timestamps while still servicing parent EOF; never steal ownership.
Claim unowned `_XSETTINGS_Sn` under a short check/claim/verify server transaction.
Publish `_XSETTINGS_SETTINGS` on the owner window before MANAGER announcement.
Return ready only after publication and required X error barriers; resource status
is independent. SelectionClear stops publication/ownership battle and retires.

EOF is canonical stop. Disable, controller close/fatal exit and parent death close
stdin; native cleanup removes only an unchanged owned resource overlay, releases
selection only if still owner, then exits. Supervisor asynchronously observes
actual Wait: allow500ms EOF grace, then TERM, another500ms then KILL. Retain the
slot until actual reap; unconfirmed retirement is stop-unknown and forbids another
launch. Parent startup bound3seconds includes native acquisition grace; apply
acknowledgment bound2seconds. Failure stops the run rather than replaying unknown
work. No crash-restart loop. Explicit retry, reload or a new explicit application
preference change can make one new attempt only after prior actual reap. Late
retired-run events never affect a newer run. Forced exit cannot promise cleanup.

## X resources

Touch only Xcursor.theme and Xcursor.size via a clearly marked private overlay in
root RESOURCE_MANAGER, never user files. At most64KiB root bytes/output, bounded
line/marker parsing, no NUL, safe logical line boundary and exact unique markers
containing run identity and owner XID. Preserve every unrelated byte. Empty owned
cursor values remove the bridge overlay rather than setting defaults. Prepare
outside a server grab, then bounded read/compare/write under a short grab; refuse
size/type/unsafe continuation inputs instead of normalizing the entire database.

Updates require the exact unchanged owned suffix. If an external writer removes,
edits or appends past it, latch resource conflict until a new explicit run; do not
restore a whole captured property. Cleanup may remove the unique exact owned block
where it remains unchanged, preserving external prefix/tail bytes; otherwise leave
it alone and report conflict. Remember any separator newline inserted by the
bridge. A new run may remove/adopt a strictly validated orphan block only after
confirming its recorded owner window is gone; ambiguous/live/reused XIDs refuse.
XSETTINGS and root resources are separate observable publications, not a claimed
cross-property atomic transaction.

## Verification and handoff

Develop the coherent slice before one focused Linux incremental build/policy/Go
normal/race/vet/actual-X checkpoint, with relevant scoped sanitizers, installed
helper lookup and generated manual. Actual new OS-specific API use gets affected
early portability checks; unchanged portable socket/process APIs alone do not boot
all BSDs. Broader six-target/release and optional portal acceptance remain open.

Cover wire/UTF-8/length boundaries, stale run/sequence, serial rollover/no-op,
empty removal, competing/lost owner, parent EOF, partial packets, descriptor and
process retirement, old schema loading, invalid save leaving saved/effective
publication unchanged, and external resource edits/oversize/orphan cleanup.
Use independent raw XSETTINGS/Xrm readers and an existing real GTK3 test client:
observe font/animation/icon/cursor values and change notifications, a fresh client,
and an enlarged-font sample. Observe fresh Xcursor resource lookup; cached client
non-effects must be reported. GTK success is not Qt/browser success. Three material
theme/font screenshots reuse that run. The due paired PR310-to-merged315 core
comparison uses already qualified cached binaries and does not qualify this helper.

Primary protocol: https://specifications.freedesktop.org/xsettings/0.5/ ; registered
keys: https://wiki.freedesktop.org/www/Specifications/XSettingsRegistry/ . Verify
additional GTK mappings against authoritative GTK source/docs before publication.
The five selected mappings were verified in the GTK3 primary source:
https://raw.githubusercontent.com/GNOME/gtk/gtk-3-24/gdk/x11/gdksettings.c .
