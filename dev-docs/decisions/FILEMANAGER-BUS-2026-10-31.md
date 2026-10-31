# FileManager1 adapter checkpoint — 2026-10-31

Owner: Cam; FILE-INTEGRATION-01 #67 and shared SESSION-02 #77. Base merged
PR314 `92742f5d5c2ed300fdb1b1a22e8f1a234a07e9d0`;
`codex/r1-filemanager-bus`. The user authorized the next coherent feature bundle.
This freezes implementation scope; it is not executed qualification.

## Outcome and simpler alternative

Add explicit `fvwm-file-browser --filemanager-service`, owning one private
`fvwm-session-bus filemanager` libdbus bridge. The existing browser Go code owns
URI-byte conversion, grouping, reuse and independent browser admission. The C
bridge owns bus parsing, bounded dispatch and name lifetime. This reuses the
accepted optional libdbus contract from PR305–308, rather than adding a Go wire
stack or duplicating browser/URI policy in C. Its resident-server admission is
new work and must be qualified independently of the transient client.

Eight related outcomes belong in one PR:

1. Explicit opt-in CLI, installed manual and source configuration example;
   no default startup,
   new preferences/registry, MIME default change or bus activation file.
2. Export `org.freedesktop.FileManager1` at `/org/freedesktop/FileManager1`:
   `ShowFolders`, `ShowItems`, `ShowItemProperties`, each `as,s` input and empty
   method return; bounded introspection/peer replies.
3. Inherit one explicit local Unix session bus, with no autolaunch or service
   activation. Request the name without replacement or queuing. Confirm ownership
   before reporting ready; conflict/absence/disabled builds remain explicit.
4. Bound incoming/native/bridge work before dispatch. One pending operation,
   no request queue; at most eight URIs, eight lexical folder groups, 16 KiB total
   URI bytes and 512 bytes startup ID. Native message64 KiB, receive128 KiB,
   outgoing64 KiB, parent frame64 KiB and reply512 bytes. Reject Unix FDs with
   bounded native FD admission. Drain at most32 messages/turn with cooperative
   2 ms work target. Backpressure is not a hard RSS or blocking-call deadline.
5. Require local file URIs on D-Bus, decode once through `filemodel.ParseTarget`,
   retain arbitrary decoded filename bytes and first occurrence order, normalize
   duplicates, validate all syntax before action, and group reveal/properties by
   lexical parent. Reject invalid/remote/root-reveal/dot/mixed-overlimit requests.
   No filesystem traversal in the service planner, target execution or shell.
6. Reuse `reuseBrowser` and its no-retry-after-unknown rule. A new independent
   browser reports private inherited-FD admission after its MFL/provider startup,
   separately from later filesystem resolution and visible presentation. Reserve
   all required launch capacity first. Admit groups in order under one shared
   deadline, so sibling discovery cannot contend for the same provider. Retain
   reservations while outcomes are
   uncertain. D-Bus success means accepted work, not completed display or mutation.
   A partially admitted multi-folder request cannot be rolled back; report it
   honestly and never retry already-submitted work automatically.
7. At most eight live browser children; one actual waiter per child, bounded
   completion channels, release only after exit. Service exit closes/stops/reaps
   only its native bridge and leaves independent browser UI alive. Pipe loss,
   bus loss/name loss and expired admission retire requests without switching
   buses or launching replacement UI. Native authentication has the existing
   five-second startup guard; parent startup/teardown is finite and reports an
   unconfirmed reap honestly. Normal request admission has a two-second deadline.
8. Independent bus-client and actual-X coverage, parser/planner/admission tests,
   scoped sanitizer/race checks, manuals and three existing material theme/font
   configurations where the browser UI is exercised. Reuse previous browser and
   platform evidence with exact unchanged-input boundaries.

The startup ID is bounded advisory data only. Forward it as a child environment
hint if supported, without claiming that an FvwmForm launched by an already
running core inherits it. Preserve existing managed presentation/focus policy;
this checkpoint does not implement startup-notification token consumption.

## Fixed private bridge protocol

C to Go: newline-delimited JSON, version1. `kind=ready` includes actual unique
owner. `kind=request` includes decimal-string monotonic nonzero `id`, one exact
method name, `uris` array and `startup_id`. Other initial outcomes use fixed
`kind=unavailable` and a bounded diagnostic reason. Frame cap64 KiB, no unbounded
stdio buffering. Go to C: version1, same `id`, fixed `status` of
`accepted|invalid|refused|unknown`, bounded `detail`; maximum512 bytes. Stale
replies cannot resolve another request. The bridge retains the pending native
message only until a matching reply, cancellation/deadline or disconnect.

Use the existing optional native helper target and disabled stub. The Go owner
uses the installed fixed helper path, with an explicitly documented absolute
`FVWM_FILEMANAGER_BUS_HELPER` override for controlled builds. The ordinary browser
CLI and default Files-menu behavior stay unchanged. No core, MFL protocol,
clipboard/XDND ownership, generic session services or settings portal expansion.

## Verification and closure boundary

Develop a complete coherent implementation before one focused Linux combined
build/unit/race/protocol/nested-X gate and affected installed helper/browser/manual
checks. Exercise all three methods with independent `dbus-send`/GIO clients,
byte-encoded paths, duplicate/group limits, malformed signatures, conflict/no-bus,
backpressure, lost replies, late outcomes, actual reuse/new views/properties,
service exit preserving browsers and WM/MFL restart. Add affected BSD compile/
process/socket checks early where required; do not repeat the Tray six-target
matrix for this new PR. Broader #67/#77 support/role qualification remains open
until its remaining frozen criteria actually pass. No whole-owner closure is
inferred from this focused feature checkpoint. #257/#258 remain parked.
