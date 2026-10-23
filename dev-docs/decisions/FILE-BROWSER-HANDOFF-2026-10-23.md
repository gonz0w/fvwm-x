# Portable browser target requests

Status: implemented; focused checks passed, current-feature integration pending. Owners: FILE-DATA-01 /
FILE-ACTION-01 and the portable subset of FILE-INTEGRATION-01. Selected after
merged PR166 and the accumulated PR164–166 integration checkpoint.

## Bounded launch boundary

- Ordinary --dir browsing accepts a literal absolute path or local file URI.
  --reveal accepts up to 256 targets in one lexical parent. --properties accepts
  exactly one target. These modes are mutually exclusive; no default entry point
  or binding is replaced. Each launch owns an independent helper/Form instance.
- Decode URI percent escapes exactly once, retaining non-UTF-8 filename bytes.
  Only file URIs with empty authority or ASCII localhost (case-insensitive) are
  local. Refuse user info, ports, remote hosts, relative/opaque forms, malformed
  escapes, NUL and raw query/fragment syntax. Raw absolute paths remain literal.
  Bound encoded input before decoding and decoded paths at 4096 bytes. Kernel
  argument limits also apply; this is not a new unbounded request transport.
- One isolated worker loads the parent and resolves every requested byte name
  to its observed ID before publishing. Duplicate/missing/mixed-parent/root
  targets or dot components fail without partial selection. Normalize repeated/
  trailing separators; never resolve a dot component across a parent symlink.
  Explicit dot targets enable hidden entries. Existing directory/wire/work and
  worker-cancellation bounds remain unchanged.
- A generation-checked adapter selection admits known domain IDs independently
  of pages, validates atomically and keeps the first requested visible target on
  screen. The native page callback retains its narrower visible-page policy.
  Properties immediately displays observed metadata, including final symlinks
  and special entries, without content reads, link traversal or activation.
  The core/consumer event loop performs no filesystem resolution.

--inspect uses the same request path and adds selected_ids in requested order
for reveal/properties. All byte paths/names retain the existing padded-base64
schema-1 fields. No selected target is executed; a later explicit Open still
uses the existing isolated revalidation and literal executable argument path.
A launch failure may be recovered through native navigation or returned as a
failed headless result; it does not silently browse another location.

## Storage boundary and remaining handoff

The user checked whether SQLite must be brought forward. It is not a dependency
of live filesystem browsing or current collection metadata. Keep domain IDs,
request/view generations and byte representations independent of database row
IDs, connection handles and table layout. File IDs describe live observations,
not durable database identities. Future CONTENT-01 owns native scoped content
handles/representations and retention; STORE-01 owns protected memory first and
optional versioned encrypted persistence/recovery afterward. No SQLite adapter,
schema, generic repository framework, plaintext clipboard database or new
mandatory dependency is introduced. Choose any backend at its owning storage
checkpoint with protected payload/preview/index and migration contracts intact.

This is the portable launch subset, not complete browser or clipboard owner
acceptance. Full scrolling/grid, focus/scale/accessibility, generic files MFL
provider, native watching, file mutation, default folder/application roles and
FileManager1 remain open. A future adapter can call these existing target/data
contracts. Clipboard raw content never enters these ordinary Form/Echo/search
metadata paths; private transfer/protection remains separately owned.

[Focused report/receipt](../verification/VERIFICATION-2026-10-23-FILE-BROWSER-HANDOFF.md).
