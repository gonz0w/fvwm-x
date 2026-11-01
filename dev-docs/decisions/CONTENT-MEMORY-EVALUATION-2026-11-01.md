# Protected content memory evaluation

Owner: Cam; #74 STORE-01 and #63 CONTENT-01. Status: source/probe preparation,
not a backend adoption or protection qualification. The owner selected
session-only history, recognized sensitive/password exclusion and persistence
off. No-hint payloads can still contain secrets, so this comparison treats all
payload buffers as potentially sensitive.

## Finite outcome and boundary

Compare dedicated base-system mappings with libsodium 1.0.22 on the same bounded
synthetic buffer/lifetime workload. Establish native ownership, page/resource
accounting, checked preparation/refusal, exact lease access and observable
cleanup before choosing the production buffer implementation. The simpler
candidate uses base-system C; the library alternative must justify dependency
cost and any unobservable failure paths. Go/Lua keep safe metadata only; managed
slices/strings or crypto APIs do not establish this native ownership contract.

This checkpoint adds no installed allocator, capture, transport, decoder,
preview, key, database, encryption, persistence or clipboard UI. The probe is
not installed, registered as a normal runtime test or linked into the WM. It
uses the existing library only for an explicitly requested candidate build.
No default Meson/dependency/feature changes or new packages are selected.

## Required buffer contract

Before accepting original bytes, a future isolated native owner must reserve
logical and physical costs, successfully prepare required locking and dump
restrictions, and hold an exact live access lease. Required protection failure
refuses preparation; no ordinary-heap fallback. Region access is closed between
bounded operations. Retired/expired leases keep cleanup obligations but cannot
authorize access. Wipe before unlock/release and preserve resource charges until
actual cleanup obligations complete. The owner must separately audit every
transport, native UI, stack/register, decoder and derivative copy.

The [native policy](../../libs/content_policy.h) now reserves mapped and locked
bytes independently of encoded/decoded/derived payload counts. Admission checks
per-item and aggregate physical caps before mutation/allocation. Replacement
charges both generations. `content_lease_check` validates exact ticket, owner,
item, generation, representation, live state and deadline immediately before
access. `content_describe` alone grants no payload access.

`content_storage_charge` calculates an explicit allocator model using supplied
page size, at least one rounded data byte and at most eight overhead pages.
It checks additions/multiplication before returning mapped/locked reservations.
This is a conservative model to attest for each actual allocator profile, not
an observed mapping/locked-memory measurement or installed capacity default.
The backend must refuse any allocation exceeding its reserved model.

## Candidate source observations

The [libsodium memory documentation](https://doc.libsodium.org/memory_management)
requires initialization and warns that allocation success does not guarantee
locking; the probe checks locking separately. Its
[1.0.22 source](https://github.com/jedisct1/libsodium/blob/1.0.22-RELEASE/src/libsodium/sodium/utils.c)
maps OpenBSD `MAP_CONCEAL` to its no-core flag, has guarded and fallback allocation
paths, and ignores several internal protection/release results. Exact configured
source/package paths still need attestation. For a page-multiple allocation,
the guarded mmap model reserves four extra mapped pages and one extra potentially
locked page. These are source-derived reservations, not observed process totals.

The baseline reserves two guard pages plus rounded data, locks only data, and
uses checked system calls. Regional controls are platform-specific:
[Linux mlock](https://man7.org/linux/man-pages/man2/mlock.2.html) and
[madvise](https://man7.org/linux/man-pages/man2/madvise.2.html),
[FreeBSD mmap](https://man.freebsd.org/cgi/man.cgi?query=mmap&sektion=2&manpath=FreeBSD+14.3-RELEASE),
and [OpenBSD mmap](https://man.openbsd.org/mmap.2).
The fetched OpenBSD manual is current, not proof against the qualified 8.0
headers/kernel; the version-specific online fetch was unavailable. Verify exact
target headers/source and behavior at the shared checkpoint. Do not advance an
OS baseline from a documentation website's default selection.

## Non-installed probes

[content_memory_probe.c](../../tests/content_memory_probe.c) builds twice:
base-system C, and `CONTENT_PROBE_SODIUM=1` against exact version 1.0.22. There
is no exported protector framework or backend registry. Both link the actual
content policy and use identical public synthetic bytes. Baseline wiping uses
the recognized Linux/BSD `explicit_bzero`; other base profiles report unavailable.
No custom wiping/cryptography fallback is introduced.

Before operations the fresh probe process verifies its own zero core-file limits.
It never changes another process or system-wide setting. A separate explicit
`--zero-lock-limit` case lowers only its own inherited lock limit and verifies
the result. This cannot override privilege exceptions or establish every dump
mechanism. Each buffer also requires a checked regional no-dump operation, or
a mapping flag the baseline itself applied. If the library's configured mapping
path is not attested and no checked regional operation is available, it reports
dump-unavailable before writing fixture bytes. This intentionally leaves the
OpenBSD library-path qualification open rather than inferring it from OS macros.

Fixture sizes are zero, one, page minus one, page, page plus one and 64 KiB.
Page size is observed and bounded to 64 KiB; repeat count is 1–20. The explicit
fixture limits (two items/leases/work tokens, 1 MiB per representation, 2 MiB
logical total, 4 MiB per-item and 8 MiB aggregate physical reservations) are
test data, not product defaults. Policy time is deterministic synthetic time;
separate monotonic measurements report real elapsed time. Wiping is inspected
while the allocation remains valid, never by reading freed memory.

Allocation/lock/dump/access injection options test cleanup control flow, not
real OS exhaustion or enforcement. Successful preparation alone permits fixture
access. The lifecycle then retires during an outstanding lease, refuses access
and early cleanup acknowledgement, releases the exact lease and observes wiping
before backend release. Sodium's void free exposes no individual unmap/unlock
result; its row records wipe-and-return, not independent physical reclamation.
Base-system cleanup failures retain charges and fail the probe.

JSON lines contain numeric budgets/timing/errno, fixed candidate identity and
preparation status only. No addresses, payloads, paths, labels or secrets.
Preparation enum: 0 prepared, 1 allocation refused, 2 lock refused, 3 dump
unavailable, 4 dump refused, 5 access refused, 6 wipe unavailable. Injected enum:
0 none, 1 allocation, 2 lock, 3 dump, 4 access. Exit 0 means the observations and
accounting completed; it does **not** mean all preparations succeeded. Exit 77
means process/library/page prerequisites unavailable, 2 invalid CLI, 1 a lifecycle,
access, cleanup, clock or observation failure. Compare only matching successfully
prepared rows; refusals remain explicit, never zero-cost benchmark samples.

## Compile and shared execution

From the source root, use an owner-controlled output directory and a configured
build's generated `config.h` directory. Compile the feature-independent policy
and probe directly; neither command executes it:

```sh
cc -std=c11 -D_DEFAULT_SOURCE=1 -I"$configured_build" -I. \
  tests/content_memory_probe.c libs/content_policy.c \
  -o "$output/content-memory-base"
cc -std=c11 -D_DEFAULT_SOURCE=1 -DCONTENT_PROBE_SODIUM=1 \
  -I"$configured_build" -I. tests/content_memory_probe.c \
  libs/content_policy.c -o "$output/content-memory-sodium" \
  $(pkg-config --cflags --libs libsodium)
```

Require the local pkg-config version to be 1.0.22 before the second build.
Capture compiler, generated configuration, flags, dependency source/package and
binary hashes. Do not install missing packages or substitute versions by this
recipe alone. Source-only macOS compilation can use the explicitly recorded
minimal header profile; it is not supported-target qualification.

At one assembled checkpoint execute the authored content policy, manager/owner
cases and the same finite probe cases with bounded external process deadlines.
Collect repeated matching baseline/library rows, init/prepare/cleanup/CPU/RSS,
mapped/locked observations and exact dependency/build/install costs. Report raw
RSS in its target-defined units and distinguish source models from measurements.
Idle activity, actual dump exclusion, compiler-resistant wiping and supported
native protection failures need their own scoped controls; probe API returns or
an exit code alone are insufficient evidence.

Publish Linux/FreeBSD/OpenBSD AMD64/ARM64 outcomes, preserving emulation,
unavailable support and gaps. Adopt/defer/reject only after corresponding evidence;
this source preparation cannot choose a backend. Native runtime and broader
release qualification remain separate. Persistence/key/unlock/recovery and
installed capacities/capture/formats/lock transitions remain explicit decisions.
