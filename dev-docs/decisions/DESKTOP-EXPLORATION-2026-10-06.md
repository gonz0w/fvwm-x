# Desktop exploration and shared primitive scope

Status: **exploration backlog and shared-contract scoping approved on
2026-10-06**. The user requested that further desktop ideas wait until all
existing work is done, while primitives with several uses are scoped into the
existing development work. This approves requirements and exploration placement;
it does not select runtime APIs, providers, dependencies or feature defaults.

Owners: DESKTOP-EXPLORE-01 for later exploration; CFG-01, IPC-02, SESSION-01/02,
FILE-VIEW-01, FILE-ACTION-01 and FILE-JOBS-01 for shared contracts, coordinated
with PROC-01, CONTENT-01, RETENTION-01, STORE-01, TEXT-01 and A11Y-01. The
[tracker](../PROJECT-TRACKER.md#execution-order) remains the only execution queue.

## Scheduling and product boundary

Complete the existing stages 1 through 8 before taking up DESKTOP-EXPLORE-01;
RELEASE-01 remains conditional on the user's request to prepare a release.
Exploration is not a release prerequisite and does not authorize publication.
Already approved session, notification, tray, settings, accessibility, file and
clipboard work retains its current placement. Existing FILE-ADVANCED-01
candidates remain deferred; this record does not promote them into release scope.

Scope the shared contracts below inside their current owning checkpoints,
using the approved session, browser, file-job and clipboard consumers. Do not
implement future hardware backends merely to demonstrate reuse. Here, core
development work includes the shared native module/service foundations; it does
not mean moving filesystem, device, bus or sensitive-content work into the WM.
Authoritative X/window state stays single-threaded in the core and managed
window actions use the shared command engine.

## Shared contracts in the current work

### 1. Capability and service-state reporting

Owners: SESSION-01/02, IPC-02 and CFG-01.

- Use a bounded versioned record for an explicitly configured provider's
  identity/generation, supported operations, lifecycle state, availability and
  failure reason. Distinguish absent, disabled, starting, ready, stale and failed;
  report authorization or protection requirements separately from availability.
- Reuse SESSION's owned/borrowed lifecycle and IPC's queries/events. Provider
  restart/disconnect invalidates old targets and pending state; subscriptions
  have limits and recover through an explicit fresh snapshot after lost updates.
- Settings and controls consume these records and expose useful disabled reasons.
  Action handlers recheck current capabilities; an enabled button is no authority.
  Start with existing desktop/session providers and preserve no-bus operation.
- This is a common contract for reviewed adapters, not an arbitrary plugin loader,
  mandatory device framework, privilege broker or duplicate settings registry.

Acceptance: native state/generation/bounds policy and independent IPC fixtures;
real-X setting/control updates on startup, failure, restart and missing providers.
Qualify owned/borrowed services and optional bus profiles with SESSION-01/02.

### 2. Typed actions and target identity

Owners: IPC-02 and FILE-ACTION-01, coordinated with CONTENT-01 and PROC-01.

- Separate operation IDs and typed literal arguments from labels. Identify the
  owning provider and target/request generations; collection IDs do not imply
  filesystem paths. Keep domain-specific payload and validation schemas.
- Report unsupported, unavailable, denied, stale and failed operations explicitly.
  Revalidate targets at execution with the owning backend; document filesystem
  race guarantees rather than treating a view generation as filesystem locking.
- Preserve legacy commands and ordering. Do not expose payloads through general
  events or command expansion; protected content uses scoped handles/leases and
  the clipboard protection contract. No arbitrary shell or FVWM evaluation.

Acceptance: file and clipboard consumers exercise the same action/result envelope
with their own schemas; test literal hostile labels/arguments, stale identities,
provider death, denied requests and sensitive-payload exclusion.

### 3. Asynchronous operation progress and cancellation

Owners: IPC-02 and FILE-JOBS-01, coordinated with PROC-01 and SESSION-01.

- Define bounded request states for accepted, running, awaiting a decision,
  completed, failed and cancelled operations. Progress reports known or unknown
  totals honestly; partial outcomes remain distinguishable from full success.
- Correlate progress, decisions, cancellation and terminal results. Reject stale
  or duplicate decisions; coalesce progress within declared bounds without losing
  terminal results. A request has one authoritative terminal outcome while owned;
  reconnection must query its status or report it unavailable, not infer success.
- Define whether work continues or is cancelled when a view/client closes, and
  how results survive within bounded service retention. Cancellation is best
  effort and neither rollback nor proof that an external side effect was avoided.
- Keep blocking work in owned isolated helpers and reuse process/readiness
  handling. Adopt the presentation contract with file jobs and session operations;
  share implementation only where their actual lifetime policies agree.

Acceptance: progress floods, unknown totals, conflicts, late/duplicate decisions,
cancellation, partial failure, helper death and view teardown; actual-X input
remains responsive. Future devices/downloads/search can reuse the contract but
are not current implementations or evidence requirements.

### 4. Collection models and bounded filtering

Owners: FILE-VIEW-01, coordinated with CONTENT-01, TEXT-01 and A11Y-01.

- Keep the list/grid presentation independent of a filesystem model. Explicit
  model adapters supply stable IDs/generations, display roles, sort/filter keys
  and permitted action references; payloads and paths retain their own ownership.
- Define bounded incremental filtering/sorting, visible-entry rendering and
  paging/admission feedback. Cancel superseded filter requests; preserve valid
  selection/focus through updates and report stale or unavailable models.
- Qualify file and clipboard adapters at their respective existing handoffs.
  Clipboard filtering/labels obey retention and protection rules; no general
  search index or plaintext sensitive-content cache is created by this contract.
- Launcher, window, theme and notification adapters are later consumers.
  Hierarchical trees, split panes, rich previews and a generic layout engine
  remain deferred until a concrete consumer justifies their scope.

Acceptance: pure model/filter/selection boundaries and real-X large collections,
keyboard access, refresh, cancellation, focus retention and scale changes. Test
file identity independently from display encoding and clipboard expiry/privacy.

### 5. Owned-setting preview, confirmation and revert

Owners: CFG-01 and IPC-02, coordinated with SESSION-01.

- Register preview support only for explicitly reversible settings with an owner,
  validated snapshot and generation. Separate saved preference, effective state
  and provisional state; define confirm, cancel, timeout and failure results.
- Bound preview duration and outstanding previews. The owner supplies recovery
  on consumer loss/restart; refuse preview when recovery cannot be supported.
  Detect intervening external changes and do not overwrite newer state blindly.
- Qualify with current owned appearance/layout settings. Future display/input
  adapters need their own topology, recovery and hardware checks before exposing
  this capability. File jobs, power actions, arbitrary configuration and remote
  side effects do not gain rollback from this contract.

Acceptance: confirm/cancel/timeout, failed apply/revert, consumer loss, owner
restart and external changes, with real-X effective-state verification and
honest failure feedback. Preserve personal configuration precedence.

These contracts use existing transport and isolated owners. Concrete syntax,
wire schemas and any implementation extraction follow source review; no broad
API rename, new general service framework or mandatory package is approved.
Use WORKFLOW.md's batched cadence and relevant SUPPORT.md native, real-X,
six-target, install/manual and sanitizer gates. Scope approval is not passing
evidence. Publish documented qualified contracts/examples at the existing
consumer handoffs; do not delay them for deferred desktop features.

## DESKTOP-EXPLORE-01: candidates after existing work

The following is an exploration inventory, not an implementation sequence.
Choose one bounded candidate after the current program; record reuse, gaps,
provider/platform availability, dependencies, ownership and acceptance before
promoting it. Prefer integration with existing application/system services.

| Candidate family | Reuse and remaining exploration |
| --- | --- |
| Desktop consumers | Richer launcher; window/workspace dashboard; wallpaper/theme browser; favorites/bookmarks/recent-item browser; clipboard snippets/templates; trash browser; operation activity panel; autostart editor; shortcut/configuration inspector; notification history/DND panel; screenshot history/browser. Reuse existing models, collections, actions and settings; decide missing data adapters and recording/privacy policy individually. |
| Display and input controls | Safe graphical RandR editing, timed recovery, keyboard layouts and pointer preferences. Reuse monitor/settings/preview contracts; explore native backend operations and topology/hotplug recovery. |
| Network and Bluetooth | Wi-Fi/VPN discovery, connections, pairing and authorization. Common capability/action UI with separately reviewed Linux/BSD providers; no mandatory daemon or credential agent. |
| Audio and media | Input/output selection, microphones, routing, change events and optional MPRIS player controls. Extend the existing volume/mute baseline through reviewed adapters. |
| Devices and power | Removable-drive mount/unmount/eject, brightness and fuller battery/idle policy. Coordinate FILE-ADVANCED and SESSION; authorization, safe removal and physical sleep/lock qualification remain backend-specific. |
| Search and advanced files | Desktop-wide provider search and optional indexing; thumbnails/previews, tabs/dual panes, bulk rename, archives, remote locations and undo. Keep FILE-ADVANCED ownership; explore decoder/cache, cancellation, privacy, recovery and provider contracts separately. |
| Application dialogs and portals | Open/Save chooser consumer and optional backend, deeper portal integration and application preference/theme consistency. FileManager1 and SETTINGS-01 remain their existing narrower scopes. |
| Credentials and authentication | Integrate available credential stores and agents; explore any new implementation separately. Protected clipboard storage is not a general keyring, password manager or authentication service. Native locker remains deferred. |
| Other desktop services | Printing/scanning, calendar synchronization/reminders, screen recording and remote desktop. Require their own provider, scheduling/capture/permission and lifecycle contracts. |
| Richer native layouts | Hierarchical trees, split panes and richer preview surfaces only when a selected consumer needs them. No replacement toolkit or aggregate UI rewrite. |

Optional tools and services must have honest unavailable/failed states and
portable fallback where supported. Preserve X11/Linux/FreeBSD/OpenBSD scope,
legacy configurations and client pixels. Physical hardware, assistive clients,
toolkits and compositor-dependent features require their own evidence. No
browser/editor/player suite, native Wayland backend or compositor is implied.
