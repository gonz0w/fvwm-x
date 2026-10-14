# Module packets: bounded lengths and temporary storage cleanup

Status: final normal/scoped sanitizer Linux and native BSD ARM64 controls pass.
This source is outside the separately frozen lifecycle acceptance checkpoint;
combined qualification remains pending. No configuration migration is needed.

## Change

Word packet construction reserves the existing header inside the existing
256-word packet capacity. SendPacket and BroadcastPacket deliver the builder's
actual bounded length. Supplied item counts no longer determine an independent
copy length after construction. Ordinary representable payloads retain their
header and values; oversized internal requests stop at the existing capacity.
The private structured configure builder remains a separate audit item.

Four named notification senders now build into fixed operation-local word
arrays. Name scanning stops at the bytes representable by the packet. The shared
builder copies that prefix and initializes the terminating/padding bytes, keeping
ordinary names, fields, truncation and packet order. Broadcast payloads remain
owned before callbacks run. Missing picture names are skipped consistently with
other named senders. There is no new library, allocation, cache or persistent
message buffer.

## Focused evidence

The prior frozen builder's ordinary short/long names match the same payload
controls in normal and scoped sanitizer builds and use four temporary allocations
per four-sender group. The new construction uses zero temporary allocations.
This is an allocation/operation-count comparison, not physical desktop latency
profiling. The output owner's required copied queue storage remains intact.

The permanent actual-core fixture checks six word counts (empty, ordinary,
maximum and beyond capacity) for both direct/broadcast delivery. Seven name
lengths exercise empty, word/padding boundaries, ordinary and truncated names
through all four senders; exact fields, zero padding, termination and declared
byte sizes are checked. Missing names emit no packet. The writer boundary is
wrapped to inspect construction independently of output queue allocations.
Existing real module output/sync/backpressure and lifetime tests remain included.

Final normal and scoped Clang ASan/UBSan profiles each pass eleven pure and
twenty-two actual-core tests. Pure units enable leak detection; startup/core
controls retain the existing leak-disabled scope. Native FreeBSD/OpenBSD ARM64
overlays compile all affected iterator owners and pass the runtime unit and the
packet fixture. The first fixture build omitted the legacy internal picture
sender's prototype; its source/logs are preserved and an explicit fixture-only
declaration fixes the build without a production edit.

Frozen source: 1343 files, four owned additions/changes from the focused
1341-file iterator source. Archive SHA-256: `de803e4d9902095493387b247451107ca34bedf5f3dd4e52738652f1dfcd6e28`.
Source/evidence: `builddir/codex-2026-10-07/module-packet-production`.
Combined six-target/compiler/install/default-X/optional qualification and native
Linux release CI remain separate; physical/toolkit/compositor evidence is unrun.

Combined qualification update: this story is locally qualified in the frozen
1346-file [transport checkpoint](VERIFICATION-2026-10-07-MODULE-TRANSPORT.md):
1312 standard entries, 120 staged identities, 124 default BSD X drivers, optional
169/149 and 48 scoped sanitizer checks. Seven complete archives validate source
hashes/modes and EOF; native release CI and physical checks remain separate.
