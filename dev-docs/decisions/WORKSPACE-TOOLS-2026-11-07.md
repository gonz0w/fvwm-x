# Named scratchpads and native overview — #221/#223

Selected for one R1 batch on 2026-11-07. Reuse core allocation identities,
committed topology, the command engine, existing palette collection/Form and
literal-argv startup. Core owns live state; Go owns observations/configuration.
No layout engine, thumbnail capture, compositor, extra daemon or preference
store. The overview is an exclusive mode of the existing palette, so it shares
one snapshot slot rather than multiplying state infrastructure.

## Shared wire and actions

Preserve existing focus/switch and release forms:

```
Send_Palette <cookie> <serial16> <generation48> <source-index>
Send_Palette <cookie> <serial16> - -
```

Add two fixed typed forms:

```
Send_Palette <cookie> <serial16> <generation48> <source-index> drag
Send_Palette <cookie> <serial16> <generation48> <source-index> move <desktop-index> <page-x> <page-y>
```

Source and destination belong to the same owned snapshot. Source is a live
window allocation; destination is a desktop row with unchanged topology and
valid nonnegative page indices. Revalidate before each fixed command-engine
step; retain no window/monitor pointer across recursive execution. Drag uses
existing interactive Move, not a Go geometry implementation. Report submitted
interactive work as `submitted:true, applied:false`, separately from observed
completion; no rollback promise.

Palette rows add structured desk/page/actual geometry, monitor output/role
identities, sticky-desks/sticky-pages/iconified/fullscreen/focused flags and
desktop rows expose page bounds. Class/instance matching stays core-private
for scratchpads, rather than enlarging every palette row or mixing live strings
into frozen facts. Existing snapshot caps and the 1 MiB total remain unchanged. Monitor identities
are decimal strings. Normal palette
consumers keep their behavior and accept the additive fields. Overview search,
switch, mark-window/move-to-selected-workspace/page and pointer move use this
same collection, stale tickets and command path. Use labels first; optional
thumbnails are not adopted because this outcome needs no capture extension,
refresh policy, protected-content handling or image-memory budget.

## Scratchpad wire and configuration

```
Send_Scratchpad v1 <cookie> <serial16> <verb> <namehex> <fingerprint64> <classhex> <instancehex-or-dash> <lease-or-dash>
```

Verbs: toggle, retry, status, launched, launch-failed. Name is at most32 ASCII
letters/digits/underscore/hyphen; class and optional instance are exact,
case-sensitive UTF-8 at most128 bytes, with no NUL/control characters. Whole
command fits the existing1000-byte command limit. The fingerprint hashes the
canonical definition including literal argv and fixed centering policy. Core
stores no argv and never constructs an executable/shell command from them.

Reply result fields: version1, name, state, desired_visible, action, lease,
target, reason. State is claimed/pending/uncertain/ambiguous/unsupported/unmatched;
action is none/launch/summoned/dismissed. Lease/target are opaque strings or
null; reasons are fixed codes. Use the existing directed JSON reply framing.
Invalid input is an explicit failure, not an accepted transition.

Up to16 named records live in one bounded core registry for the WM lifetime.
Names are not silently evicted and no unregister verb is selected; full WM
Restart retires unused names. Inactive same-name definitions can change only
without a target/lease; active fingerprint changes require that recovery boundary.
A live claim wins; otherwise exactly one match can be claimed. Multiple matches
refuse without launch. Internal/retired windows cannot be claimed; an otherwise
matching transient/sticky/fullscreen target refuses relocation without changing
its flags or launching another app. A competing live owner or changed active
fingerprint conflicts rather than stealing a claim. Claim identity is the core
allocation identity, never XID or a display row index. Replay does not toggle
again or issue launch permission twice.

Only the first reservation returns action=launch. Go launches literal configured
argv through its existing startup path, then acknowledges the lease; definite
start failure can retire that reservation. Pending30-second leases coalesce
rapid toggles into desired visibility. Expiry/helper loss becomes uncertain,
not automatic relaunch. Explicit Retry performs a fresh scan before authorizing
another launch; it can duplicate an exceptionally late original application,
so that uncertainty is visible. Full WM Restart captures the existing restart
flag before reset and starts unmatched definitions uncertain; existing matches
remain claimable. A fresh core startup permits initial launch. No journal or
new data store is required. Different-WM replacement is not an automatic
recovery guarantee.

Summon uses the current selected monitor/desk/page and fresh reachable work area,
centering the existing actual-pixel frame without resizing the client. Dismiss
uses existing iconification. Save prior focus as a stable identity; restore it
only if still live and the scratchpad still owns the focus transition. Manual
focus changes are respected. Topology is revalidated at action time.

Module configuration supplies named class/optional-instance and literal argv
through the existing word parser:

```
*FvwmDesktop: Scratchpad NAME CLASS INSTANCE|- COMMAND [ARG...]
```

Public module commands are overview, scratchpads, scratchpad NAME and
scratchpad-retry NAME. The bootstrap and dynamic launcher provide entries;
Super+O opens the overview without replacing existing bindings. No old binding/function/directive changes. Event masks are the
union of active consumers: closing an overview cannot disable scratchpad work.

## Acceptance and exclusions

Write pure parser/policy/lease regressions, then implement the whole batch before
combined native checks. Cover two configured applications, ambiguity, delayed/
failed/rapid/replayed launch, helper/full-core restart, identity reuse, unsupported
flags, focus ownership, topology/scale changes, large collections, stale targets
and teardown. Capture keyboard/pointer interaction across three configurations.
No fresh benchmark campaign or full platform/install matrix for creating a PR;
required affected safety/target checks and later release gates remain binding.
Issue closure requires each frozen criterion mapped to actual evidence.
