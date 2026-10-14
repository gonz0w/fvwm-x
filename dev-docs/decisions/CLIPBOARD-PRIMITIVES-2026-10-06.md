# Configurable clipboard manager and protected content primitives

Status: **primitives-first direction and backlog approved on 2026-10-06**. The
user requested reusable primitives for a text/image clipboard manager, with
filesystem and memory protection suitable for frequently copied passwords. A
separate user-directed thread can consume qualified primitives to build the
configuration/theme in parallel with further runtime work. This records scope;
no runtime implementation, public syntax, dependency or security qualification
is selected or claimed. Password retention and persistence defaults remain open
decisions. Order and status belong to the
[project tracker](../PROJECT-TRACKER.md#execution-order).

Owners: CONTENT-01, RETENTION-01, STORE-01 and CLIPBOARD-01. Reuse SELECTION-01
for X11 transfer, FILE-VIEW-01 for collections, FILE-ACTION-01/IPC-02 for typed
actions/results, and CFG-01 for settings. Coordinate with DATA-01, IMAGE-01,
INPUT-01, PROC-01, TEXT-01, MODULE-SCALE-01, A11Y-01 and SESSION-01.

The subsequent [runtime Go/Lua decision](RUNTIME-GO-LUA-2026-10-06.md)
adds a qualified isolated scripting consumer of these shared contracts.
Retention/actions may expose supported metadata and scoped handles; sensitive
payloads, keys and image derivatives do not enter Go/Lua managed heaps by
default. STORE-01 retains protection review and no confidentiality/default
decision is changed by the language direction.

## Product direction and existing foundation

Users assemble the manager from FVWM configuration, bindings, menus and native
modules. Configuration controls presentation, capture rules, retention, formats
and explicit actions. Runtime primitives enforce transport, lifetime, storage
and protection requirements. Ship a small working consumer example at the first
qualified checkpoint; the full configuration/theme is a separate workstream.

The [file-manager plan](FILE-MANAGER-PRIMITIVES-2026-10-06.md) already scopes a
structured collection view, stable IDs, bounded updates and typed actions. Reuse
these contracts with content-backed entries rather than inventing another
list/grid or making clipboard items impersonate filesystem paths. File mutation,
directory enumeration and advanced file thumbnails are not clipboard
prerequisites.

Existing FSelectionRequestText is a module-only synchronous format-8 text
request with a 32 KiB cap and one-second maximum timeout. It is not evidence of
an asynchronous clipboard owner, large/image transfer or protected history. Keep
existing callers compatible; audit helpers and native consumers before choosing
additive implementation entry points. SELECTION-01's general transfer audit
remains its own work; the new clipboard consumer explicitly scopes the service
previously excluded from that item alone.

Generic reuse candidates include snippet libraries, screenshot collections,
drag-and-drop shelves and expiring local transfer queues. Keep the first
contracts driven by these concrete needs and the clipboard consumer; this is not
an arbitrary database, password-vault framework or plugin platform.

## Security boundary and ownership

Protect retained data from unintended readable files, logs, unnecessary memory
copies, stale buffers and supported swap/core-dump exposures. X11 selections are
ordinarily accessible to other clients on the same display. This design does not
promise clipboard confidentiality against a hostile display client, root, a
compromised kernel/process, physical memory acquisition or hibernation images.
Document actual OS/profile guarantees and remaining limits.

An isolated module/service owns content, retention and storage. Keep secret
payloads out of WM state, command expansion, general JSON event fan-out and
unrelated consumers. Reuse IPC-02 correlation/framing and owned process helpers;
audit which existing paths can carry sensitive payloads before adopting them.
Use bounded direct content delivery only to explicitly selected consumers.
Private endpoints, same-user access checks and process separation reduce
accidental exposure; they are not a sandbox against hostile same-user code.

Authoritative managed-window state remains in the single-threaded core, with
managed-window actions through the shared command engine. Disk I/O, search,
crypto derivation and decoding stay outside the WM loop. One blocking call
cannot be contained by an event-loop time budget; define cancellation, helper
termination and bounded completion with existing ownership mechanisms.

## Reusable primitive contracts

### CONTENT-01: typed content and identity

- Stable item IDs and generations separate from display labels, previews,
  filenames and payloads. Define ownership, immutable snapshots/leases and
  stale-reference refusal across expiry, replacement and teardown.
- Multiple bounded representations per item: text, actual image bytes and
  literal file/URI references. Preserve original bytes separately from display
  normalization. A preview or image filename is not the original image payload.
- Carry sensitivity and lifetime policy across every representation and derived
  preview. Never retain a secret's alternate format after rejecting its text.
- Bound target counts, per-representation and aggregate bytes, retained items,
  outstanding requests and derived resources. Deduplication/pinning must not
  bypass limits or merge items with incompatible sensitivity/lifetime policies.

### SELECTION-01: asynchronous X11 transfer adoption

- Define module source/requestor/owner roles and owner-change observation.
  Review the existing optional XFixes capability and an honest unavailable or
  bounded fallback profile; do not introduce a dependency silently.
- Qualify TARGETS, TIMESTAMP, MULTIPLE, INCR, text encodings and image targets
  with independent clients. Specify chunk/total limits, deadlines, timestamps,
  generation checks, cancellation, owner/requestor death and late replies.
- Define CLIPBOARD_MANAGER/SAVE_TARGETS adoption and competing-owner behavior
  against the specification. Do not silently displace an existing manager. Keep
  persistence requests subject to sensitivity and admission policy.
- Keep CLIPBOARD and PRIMARY capture/synchronization independently configurable.
  Preserve source clearing and expiry; avoid re-capturing our own publication,
  restoring an expired secret or clearing a newer unrelated clipboard value.
- File URI/cut representations reuse the file-manager transfer contract. Copying
  content never executes it or implicitly performs a filesystem operation.

### RETENTION-01: user-controlled admission and lifetime

- Typed settings for capture/pause, permitted formats, source exclusions,
  sensitivity, expiry, pin/unpin, clear and lock. Keep current selection
  ownership, retained history and disk persistence as separate states and
  operations.
- Honor supported secret hints, including x-kde-passwordManagerHint, before
  history insertion, previews and automatic actions. Treat malformed hints and
  disappearing owners explicitly; detection cannot identify every unmarked
  password. Window-title filters and pattern guesses are policy aids, not trust.
- Define lock/unlock, timeout, logout/restart and service-failure behavior.
  Expiry removes owned payloads, alternate formats, previews and indexes and
  cancels pending work; retained leases must not resurrect an expired item.
- Ordinary events expose minimal metadata. Sensitive content is absent from
  notifications, titles, logs, search snippets and automatic scripts. Export,
  editing or external processing is an explicit payload-release operation with
  documented consequences and a defined policy, not a default event hook.

### STORE-01: protected memory and optional persistence

- Minimize plaintext copies and lifetimes; use checked memory locking for keys
  and required sensitive buffers, reliable non-optimized wiping and supported
  dump/debug restrictions. Audit library, transport, decoder and UI copies too.
  Report capabilities and refuse a requested protection mode if its guarantees
  cannot be met; do not silently downgrade after a locking/allocation failure.
- Large images need separate encoded/decoded/preview budgets. Evaluate bounded
  encrypted retention in memory and short-lived plaintext views rather than
  assuming all image history fits locked RAM. Encryption in RAM does not protect
  against an attacker who can inspect the running process and its keys.
- Persistence uses private owned directories/files, safe descriptor/path and
  symlink handling, authenticated encryption, validated bounded formats, and
  atomic recovery. Protect payloads, labels, previews and search indexes;
  document any deliberately exposed size/timing metadata. No plaintext temporary
  files, thumbnail cache or journal may undermine the advertised mode.
- Define key derivation, unlock/relock, key rotation, migration, corruption and
  failed-write behavior. Do not store an unprotected key beside ciphertext and
  describe that as protection from filesystem disclosure. A user passphrase and
  optional existing key-store integration are candidates, not selected defaults.
- Prefer an established crypto/secure-memory library after dependency/platform
  review; libsodium is a candidate, not an adopted requirement. No custom
  crypto, mandatory keyring/desktop framework or silent plaintext persistence
  fallback. Document feature availability in Meson/install profiles before
  adoption.
- Deletion has explicit limits for SSDs, journals, snapshots and backups.
  Consider key erasure where appropriate without claiming rollback-resistant or
  physical secure deletion. Session-only data never enters persistence, exports
  or backups implicitly, including after a later settings change.

### CLIPBOARD-01: consumer adapter and documented handoff

- Join the qualified content/transfer/retention/store contracts to FILE-VIEW-01
  and typed actions. Publish search/list/select/copy/delete/pin/pause/lock
  capabilities with correlated results and stale-target refusal; actual syntax
  is selected during implementation, not by these operation descriptions.
- Audit and reuse IMAGE-01's bounded workers/lifetime/scale contracts for image
  previews. Sensitive inputs and derived pixels need a separate privacy review
  before use of shared caches. No automatic rich-HTML execution, remote fetching
  or content-derived command evaluation. Begin with bounded plain-text/image
  presentation; additional rich formats need individual adoption decisions.
- Provide keyboard access, focus return, text/image previews, clear
  limit/failure feedback and a minimal configuration example. Content is
  selected by stable ID, never reconstructed from a truncated label or passed
  through shell expansion.
- Optional automatic paste needs an explicit target/focus contract; putting an
  item on CLIPBOARD is not proof it was pasted. A selection request is not proof
  of an intentional user paste, so do not infer safe single-use secret deletion.

## Ranked checkpoints and complexity

These P2 additions follow current hardening/responsiveness and shared desktop
contracts in stage 6. SELECTION-01 audits/adoption keep their stages 3/5 owners.
No D-Bus, tray, file mutation or advanced file-manager feature is required for
the first clipboard handoff. Feature-local ranks do not create another queue.

| Rank | Checkpoint | Relative complexity |
| --- | --- | --- |
| 1 | CONTENT-01, qualified SELECTION-01 transfers, RETENTION-01, STORE-01 memory protection and CLIPBOARD-01 minimal text/image consumer using FILE-VIEW-01 | High overall; transfer/lifetime/protection dominate, collection UI is reused |
| 2 | STORE-01 encrypted persistence with unlock/relock/recovery and consumer settings/actions | High; key management and every derivative/storage path need qualification |
| Decision-dependent | Optional hidden, expiring secret history if explicitly selected | High assurance burden; isolation, protection failures and lifecycle must be demonstrated |
| Later | Individually adopted rich formats, transformations or other content consumers | Variable; no synchronization/cloud service or general automation platform implied |

At rank 1, publish installed man pages, actual supported syntax, versioned
item/action/result contracts, protection/limit capabilities, exact
source/profile evidence and a working text/image example. The user can then
build the full configuration/theme in a separate thread while persistence or
other runtime checkpoints proceed. Publish incremental qualified handoffs rather
than waiting for every enhancement. Prototype/mock interfaces are not stable
runtime promises.

The configuration consumer owns layout, palettes, bindings and workflows; the
runtime owner retains service correctness and security. Preserve each other's
source/build/evidence and return missing primitives to their owner. This record
creates no chat, starts no service and changes no default configuration.

## Open decisions before implementation/adoption

The user approved scope and ordering, not answers to these policy questions:

1. Whether marked passwords bypass history or can enter an explicitly selected
   hidden, expiring protected-memory history. Secret disk history is not
   implied.
2. Whether ordinary text/image persistence is enabled by default or opt-in, plus
   unlock credentials, lock triggers and behavior while storage is locked.
3. CLIPBOARD versus PRIMARY defaults, source exclusions, numeric byte/pixel/item
   limits and deadlines, pin behavior at capacity and supported image/extra
   target formats.
4. Crypto backend/feature packaging, OS-specific protection capabilities and
   conditions requiring refusal. Portable browsing/login must remain available.
5. Component boundaries, public syntax, sensitive payload delivery and any
   explicit export/external-action or automatic-paste contract.

Recommended starting policy for design review: capture CLIPBOARD only, keep
PRIMARY synchronization opt-in, exclude marked secrets unless protected-memory
retention is deliberately selected, and make encrypted persistence opt-in. These
recommendations are not recorded as adopted defaults.

## Acceptance and qualification

Follow WORKFLOW.md's focused-development/combined-qualification cadence and the
SUPPORT.md/BSD.md gates. Pure native coverage includes representation identity,
policy precedence, expiry/pinning/bounds, stale generations, ownership and
failure cleanup. Independent real-X clients exercise negotiated text/images,
large/malformed/stalled transfers, secret hints, competing managers, owner
death, clear/replace races and unrelated WM responsiveness under load.

Native storage fixtures cover permissions, symlink/replacement attacks,
partial/corrupt files, full/denied storage, migration, key changes, lock/unlock,
helper crashes, protection failures and leakage to caches/temp files/logs.
Demonstrate enforced locking/dump policy on each advertised OS/profile and
bounded plaintext handling rather than inferring protection from an API call.
Check actual password-manager/browser/image-editor interoperability separately;
unmarked-password detection is never a claimed guarantee.

Qualify supported Linux/FreeBSD/OpenBSD on AMD64/ARM64, installed
manuals/examples, enabled/disabled feature profiles and native real-X
scale/focus/teardown behavior. Native Linux ASan/UBSan CI remains separate from
emulated/local evidence. Include security review of the retained-data/key
lifecycle before protection claims. A polished configuration prototype is not
runtime/security qualification.

## Reference designs and protocols

Reviewed for this discussion on 2026-10-06; upstream development documentation
may describe capabilities not present in installed distribution releases.

- [CopyQ security](https://copyq.readthedocs.io/en/stable/security.html): secret
  hints and configurable capture; ordinary history is unencrypted unless
  enabled.
- [CopyQ password protection](https://copyq.readthedocs.io/en/stable/password-protection.html):
  encrypted history/key-store choices and separate export behavior.
- [GPaste](https://github.com/Keruspe/GPaste): named password items, timeout,
  text/images and optional encrypted persistence in current development docs.
- [Klipper configuration](https://docs.kde.org/stable_kf6/en/plasma-workspace/klipper/preferences.html):
  selection/clipboard controls, persistence and image data versus file
  references.
- [cliphist](https://github.com/sentriz/cliphist): picker/storage separation,
  stable IDs and original-byte preservation; Wayland transport is not adopted.
- [ICCCM](https://xorg.freedesktop.org/archive/X11R7.7/doc/xorg-docs/icccm/icccm.html)
  and [clipboard manager specification](https://wiki.freedesktop.org/www/ClipboardManager/):
  X11 selection and manager contracts.
- [X.Org security discussion](https://xorg.freedesktop.org/wiki/Events/XDC2014/XDC2014DodierPeresSecurity/xorg-talk.pdf):
  ordinary X11 selection access lacks user mediation.
- [Libsodium secure memory](https://libsodium.gitbook.io/doc/memory_management),
  [authenticated encryption](https://libsodium.gitbook.io/doc/secret-key_cryptography/aead)
  and [password derivation](https://libsodium.gitbook.io/doc/password_hashing):
  established implementation candidates and protection limitations.

These references inform requirements; they select no dependency and establish no
security audit or qualification claim for FVWM-X or another manager.
