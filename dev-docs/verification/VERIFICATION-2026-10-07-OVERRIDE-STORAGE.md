# STACK-01: unmanaged correction storage ownership

Status: locally qualified in the combined 1332-file
[storage checkpoint](VERIFICATION-2026-10-07-STACK-BATCH.md). The original
1330-file focused evidence is retained separately.
No configuration compatibility approval is pending.

The user reiterated configuration-file/format-only compatibility on 2026-10-07.
The previous approval hold for allocation failure was overbroad and is withdrawn;
see the [current scope](../decisions/CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
Existing BugOpts/commands/configuration need no edit or migration for this change.

## Implementation

The checked window buffer now has an already-counted-prefix constructor shared
with Layer's ring preparation. Count, spare-slot addition and byte multiplication
are validated before allocation; failure initializes an empty owner. Existing
prepare/finish ownership and Layer spare slots remain intact.

RaiseOverUnmanaged owns that buffer synchronously, packs only the selected prefix
and frees it once. Allocation failure logs and skips only the extra unmanaged
correction. Completed managed Raise remains intact, and a later Raise can retry.
Successful selection, icon packing and X request order remain unchanged. No
library, thread, global registry, new configuration name or argument is added.
The manual documents the resource-failure behavior under the existing option.

## Evidence and limits

Exact narrow source: 1330 files, eight owned changes from the accepted Layer
snapshot, in `builddir/codex-2026-10-07/override-storage-production`.
The constructor unit covers zero/unrepresentable counts, arithmetic refusal,
small forced allocation refusal, successful prefix/spare-slot capacity and
cleanup. The new real-frame driver has four cases: one/two managed records,
ordinary allocation/refusal, preserved managed/server order and successful retry.
The two-record case confirms already-completed managed Raise remains published.

Seven pure and thirteen actual-core tests pass in the focused Linux build and
the same scoped Clang ASan/UBSan profile (pure leaks enabled, core leaks disabled).
Existing Layer, packing, parent, transient, query and real-client unmanaged/focus
integration checks are included. Both native FreeBSD/OpenBSD ARM64 checks pass
the buffer unit and all four new real-frame cases in isolated overlay builds.
The accepted 1328-file qualification trees are preserved. The combined
1332-file checkpoint qualifies these changes with 1256 standard entries, 120
installed identities, 104 default-X invocations, optional 162/142 entries and
41 scoped sanitizers. Native release CI remains separate. Private
baseline/candidate evidence remains archived.
