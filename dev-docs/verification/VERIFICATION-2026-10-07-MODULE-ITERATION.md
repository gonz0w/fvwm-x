# Module broadcast continuation after retirement

Status: implemented; final normal/scoped Linux controls pass. Native FreeBSD/OpenBSD ARM64 complete-header overlays also pass the runtime
unit and both actual-core broadcast cases. This source is outside the frozen
lifecycle acceptance checkpoint; combined qualification remains pending.
No configuration migration is needed.

## Finding and owner

A bounded pinned-producer unit control retires the upcoming middle member after
an iterator has yielded the first member. The prior source visits the retired
member and misses the surviving final producer. Both normal and scoped controls
reproduce that result. This is a delivery/continuation finding; the control does
not release its pinned producer or attempt an external-system action.

An iterator now holds cleanup until exhaustion or explicit finish. Retired list
records retain their original successor and use a separate death-row link.
Iteration skips retired members and can still reach surviving members after
nested retirement and cleanup. New registrations remain outside an already
started traversal. No snapshot allocation, registry, X request or list rescan
is added; traversal stays linear and retirement stays allocation-free.

Five early-exit owners release their hold explicitly: pending configure lookup,
single configure flush, startup command-line lookup, direct monitor publication
and interaction eligibility. All other core iterator call sites exhaust their
traversal. A nested iterator cannot release an outer iterator's cleanup hold;
existing queued/active input pins still apply after all iterators finish.

## Focused coverage

Pure units cover a retired future member, survival/order, nested kill-all and
cleanup, explicit/idempotent finish, repeated empty exhaustion and registration
during traversal. Actual-core BroadcastPacket controls retire the upcoming
producer or all producers from the first writer callback. The survivor receives
its exact event payload through a real pipe; retired descriptors close after
iteration unwinds. The callback boundary is wrapped; existing actual synchronous
transport checks remain included.

Final normal and scoped Clang ASan/UBSan profiles each pass eleven pure and
twenty-one actual-core tests. Pure units enable leak detection; core startup
controls retain the existing leak-disabled scope. Native overlays compile every
core owner which allocates an iterator, using one complete header definition.
The initial native overlay selected a baseline header through a source-relative
include; the compiler errors are retained. Corrected overlay compilation adds
its generated config and new iterator header before dependent includes, with no
production change.

Four focused/native/baseline archives validate complete payload hashes, modes
and compressed/archive EOF.

Frozen source: 1341 files, nine owned additions/changes from the separate
1339-file lifecycle source. Archive SHA-256: `90bbb1fc46f8b25f52a33a4a4f00567ee3bed7df7718149cc86f83504e878e35`.
Source/evidence: `builddir/codex-2026-10-07/module-iteration-production`.
Bounded baseline: `builddir/codex-2026-10-07/module-iteration-review`.
Combined six-target/compiler/install/default-X/optional qualification and native
Linux release CI remain separate; physical/toolkit/compositor evidence is unrun.

Combined qualification update: this story is locally qualified in the frozen
1346-file [transport checkpoint](VERIFICATION-2026-10-07-MODULE-TRANSPORT.md):
1312 standard entries, 120 staged identities, 124 default BSD X drivers, optional
169/149 and 48 scoped sanitizer checks. Seven complete archives validate source
hashes/modes and EOF; native release CI and physical checks remain separate.
