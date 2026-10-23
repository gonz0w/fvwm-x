# Saved launcher-opacity preview

Status: implemented; focused checks passed, shared integration pending. Owners: CFG-01 / IPC-02.
Continues merged PR163's core runtime lease within one owned scalar preference.

## Registered value and data format

Add launcher_opacity as an integer 0–100, default 94. It applies only to the
existing SlateLauncherStyle when transparency is enabled. Opaque mode presents
100% but retains the preference; explicit module override or opaque mode refuses
saved-setting preview. Existing FVWM commands/configuration keep their contract.
MFL forwards up to eight string/boolean/integral signed-32-bit values and the
preference owner validates actual keys/types/ranges. No packet type changes.

Unversioned data keeps the previous normalization. Complete schema-1 files load
with the new default without writing; strict field validation remains. Explicit
saves write schema 2. No manual migration is required; older helpers cannot read
schema 2. The private nonblocking writer lock, revision check and atomic replace
remain authoritative. Invalid/future data is preserved.

## Confirmation is not a file transaction

Native preview snapshots settings generation and file revision, then rechecks
before core application and confirmation. The core lease must confirm first.
Only afterward may the owner persist this one field, repeating revision checks
under its writer lock. A refusal before confirmation never saves. If persistence
fails after confirmation, report runtime kept / preference not saved. Do not
pretend the core/file pair was atomic or blindly replay/rollback newer policy.

After file commit, update the in-memory preference and theme-key opacity without
resubmitting Colorset/Style/MenuStyle commands. Core state can change after its
confirmation; rendering remains unknown. Future ordinary theme changes may
apply the saved preference. Generic IPC saved-preview capability remains false;
the registry identifies the separate native preview consumer.

## Bounded native draft handoff

F4 validates/captures at most the eight known editor fields and retires its old
Form/token. Only opacity is tried. Preview completion recreates the captured
draft under a fresh editor token when settings freshness permits; a successful
scoped save returns to the remaining unsaved fields. Newer settings discard an
outdated draft. Revert/expiry never persist the draft or opacity. One core lease,
one reply wait and one queued control retain their existing limits; Revert wins.

The native preview Form also has a 16-second Nop timeout. It closes the orphaned
consumer after helper loss without touching preferences or confirming the lease.
The core's independent deadline/module retirement still owns actual recovery.
Automatic window retirement no longer invalidates freshness when it removes
no WindowStyle definition. That false conflict was reproduced by closing the
predecessor editor during preview. Real definition removal and explicit destroy
commands still invalidate. Native policy observation checks both lifetimes.
Full appearance/image resources and general saved-preview adapters remain open.

The partial-outcome notice uses the native centered default. Screenshot review
reproduced the inherited negative-position placement below the visible screen;
centering makes confirmation/write-failure outcomes readable. The numeric field
uses a literal “percent” label because Form consumes a percent substitution.

[Focused verification](../verification/VERIFICATION-2026-10-22-SAVED-OPACITY-PREVIEW.md).
