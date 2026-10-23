# File collection controls

Status: implemented; focused checks passed, shared integration pending. Owners: FILE-VIEW-01 /
FILE-ACTION-01, shared with CONTENT-01's future metadata consumer. This is the
next selected supporting batch after merged PR165, using the approved optional
Go runtime and existing native Form consumer.

## Coherent checkpoint

1. A bounded metadata projection adds case-insensitive literal-label filtering,
   a collection generation, empty-result/page counts and a visible
   anchor across same-folder refresh. Replacement/filter publication validates
   before mutation. The admitted snapshot remains capped at 4096 entries; query
   text is valid UTF-8, at most 128 bytes, with no NUL. The search is a substring
   of the presented metadata label, not arbitrary content or raw filename bytes.
2. The file adapter sorts admitted observations by name, kind, size or modified
   time in either direction. Directories remain first, ties use literal filename
   bytes, integer comparisons do not subtract, and domain observations/IDs are
   unchanged. No filesystem calls occur in these projections. Sorting resets the
   viewport; refresh preserves its first surviving visible anchor.
3. Select page and Clear selection operate on stable domain IDs with the existing
   256-item cap. Filtering preserves excluded selections and explicitly counts
   them. Selection can span pages; clear includes excluded items. A failed
   whole-page admission does not partially select its rows.
4. A selected item's inspector displays the last successful scan's name, kind,
   mode, size and UTC modification time. It is explicitly observed metadata,
   including symlinks/special entries, never a fresh stat or target lease. It
   reads no file content and follows no link. Opening retains the worker's
   separate revalidation and literal process argument contract.

All rendered callbacks still bind instance/form generation. Reordering or
filtering invalidates old controls at the next render. The collection model
also advances its own generation on projection changes. Duplicate labels never
supply action identities. Filters/order persist across navigation; selection
is cleared when the directory changes and pruned to surviving IDs on refresh.
Metadata-focus policy is preserved where visible, but native Form keyboard focus
across window replacement remains an open full-widget acceptance item.

## Limits and verification

No C, module packet, settings schema, install/discovery or server-dependent
pointer/rendering contract changes. Existing Go-enabled packaging is retained.
Focused Linux policy/race/vet, incremental helper/manual build and actual-X
consumer regression/screenshots qualify this feature PR. The accumulated
PR164/165/current affected six-target checkpoint is due after the third merge,
before another implementation PR. Full compiler/profile/install/default-X and
native-CPU/hardware coverage retain their acceptance/release owners.

The full scrollable/grid browser, external structured files provider, automatic
watching, mutation jobs, protected clipboard ownership/transfers and private
payload delivery remain open. Clipboard may reuse this metadata policy only;
secret content must not enter its labels, search indexes or general Echo/Form
callbacks. No new persistence, toolkit, mandatory service or default binding.

[Focused report/receipt](../verification/VERIFICATION-2026-10-23-FILE-COLLECTION-CONTROLS.md).
