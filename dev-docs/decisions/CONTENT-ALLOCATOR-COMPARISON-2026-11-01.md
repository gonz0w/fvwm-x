# Protected allocator comparison decision

Owner Cam; #74 STORE-01, coordinated with #63/#71. Status: retain the
base-system mapping direction for the next bounded native payload batch;
**defer libsodium guarded-allocation integration**. This completes only the
selected allocator comparison. It adopts no installed backend, dependency,
numeric defaults or persistence/key policy.

## Basis

The [six-profile comparison](../verification/VERIFICATION-2026-11-01-CONTENT-MEMORY-TARGETS.md)
observes both candidates preparing 360/360 normal fixtures per profile, matched
unprivileged positives, actual native access faults and genuine zero-lock-limit
refusals. It binds exact official libsodium 1.0.22 source, configured utility
macros, allocator object/archive, static linkage and native compiler/artifact
identities. AMD64 CPU emulation and physical-host gaps remain explicit.

Base mappings expose checked allocation, protection, locking, unlocking and
unmapping results. Sodium still needs the same project admission, exact
producer/reader access, lifetime accounting and explicit locking checks. Its
void free cannot independently report unmap/unlock outcomes. Both optimized
fixtures observe wiping before release; neither establishes protection of
future temporary, transport, decoder or UI copies.

Sodium supplies guarded allocation and canary handling. The finite comparison
finds no required capability gain that outweighs its added dependency, larger
source-derived reservations and less observable cleanup. The base model reserves
two guard pages beyond rounded data; sodium reserves four extra mapped pages
and one extra locked page. These are models, not measured physical totals.

Sodium has higher matched median prepare and finish phase costs in these
fixtures. Debug-containing comparison binaries are approximately 67–136 KB for
base and 990–1,954 KB for sodium. Its complete private SDK is approximately
3.54–6.63 MB, with 9,072,098 source bytes. Exact bytes and variability are in
the report. No production install delta or WM performance gain is inferred.
No runtime application code was removed by this evaluation.

## Consequence and next boundary

Implement one native buffer owner using the existing content reservation,
generation, producer and reader contracts with these qualified base facilities.
Keep metadata-only Go/Lua paths and refuse required protection failure without
an ordinary-heap fallback. Model and retain charges until all cleanup obligations
complete, including partial unlock/unmap failures. Avoid a generic allocator
framework or alternative-backend registry. Review and qualify that actual owner;
passing comparison probes do not grant its runtime acceptance.

Sodium guarded allocation remains an evaluated alternative, not a rejected
security library. Optional authenticated encryption, relevant Go crypto APIs,
key/unlock/recovery/migration and protected copy-boundary integration remain
separate #74 work. No custom cryptography follows from choosing base memory
facilities. Session-only history, recognized sensitive exclusion, persistence
off, CLIPBOARD-only capture, PRIMARY sync off and history retained while locked
remain owner defaults. Lock-time capture/access and installed capacities remain
separate until the owner decides them.

Linux/FreeBSD checked regional advice, OpenBSD's baseline mapping flag and its
sodium configured-source attestation are distinct evidence. No dump collection
or universal dump-exclusion claim is made. Main-entry initialization excludes
loader startup; process raw RSS is not allocator accounting. Zero raw readings
are not zero memory use. Product idle cost, production integration/installation,
actual capture/UI and release-wide acceptance remain open.

Separate source/dispatch/evidence review supports this bounded direction;
agent review is not independent human assurance. Continue R1 under the existing
reviewed-checkpoint merge authority. #63/#71/#74 remain open; #257/#258 stay
parked. No platform or configuration scope changes.
