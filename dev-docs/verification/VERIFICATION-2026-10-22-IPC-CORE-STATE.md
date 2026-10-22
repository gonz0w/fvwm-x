# Directed core state queries and accumulated IPC integration — IPC-02

The three-feature core-query batch and a reproduced provider-readiness fix pass
one combined affected cycle across six targets/nine profiles. The candidate
integrates merged PR155 settings and PR156 validation/wait lifecycle work.
[Exact source/profile receipt](VERIFICATION-2026-10-22-IPC-CORE-STATE.json).

Base: merged PR156, `45a164ac22b6278a490b233c421ceaaf44190d41`.
The receipt binds every changed runtime/test/build input, actual executable
identities and two frozen source archives. The readiness correction changes
only the desktop helper, its policy tests and settings X fixture; core/MFL and
other fixture inputs stay exact. Documentation/status edits remain outside
those runtime inputs. [Contract](../decisions/IPC-CORE-STATE-QUERIES-2026-10-22.md).

## Resulting behavior

- `topology` observes the existing committed RandR snapshot, including geometry,
  physical dimensions, outputs, primary flags, explicit root fallback and a
  lossless hexadecimal epoch. No X acquisition or second state authority.
- `workspaces` observes core monitor desks, viewport/page grid, scale quarters
  and separate static/dynamic work-area caches. Freshness is explicitly cached;
  ambiguous all-zero areas are null/unknown. No window scan or publication is
  forced by a query, and separate queries are not an atomic server transaction.
- `bindings` copies bounded pages of global Key/PointerKey/mouse records. An
  incarnation/revision token rejects stale continuation, including after core
  restart; binding commands conservatively invalidate pages. Actions are literal
  data. Menu/placement bindings and per-event matching stay outside this subset.
- The cycle reproduces a settings startup race: a request can overtake the
  helper's initial module configuration and expose provisional defaults without
  overrides. Typed queries, validation and mutations now return
  `provider_not_ready` until END_CONFIG_INFO. A bounded read-query retry in the
  fixture waits for the configured owner; actions are never automatically replayed.

Internal `Send_Query` uses the existing shared command engine and directed
MX_REPLY packets. Snapshot results are bounded to 32768 bytes/128 ordered chunks,
64 monitors/64 outputs per monitor, and checked string/page/scan limits. One
assembly owner handles core and helper replies with distinct prefixes/admission;
settings retain their 8192-byte bound. Existing eight pending slots, absolute
five-second deadlines, read-only queries, pending inspection and wait-only
cancellation apply. No pointers survive command evaluation, no new endpoint or
mandatory dependency, and no native packet layout or configuration migration.
Shipped Perl command metadata follows the additive internal command.

## Actual verification and composition

The final composed checkpoint has **113 selected passes across six targets and
nine profiles**: Linux ARM64 GCC/Clang/minimal and AMD64 GCC/Clang, FreeBSD 15.1
ARM64/AMD64, and OpenBSD 8.0 ARM64/AMD64. The 13 Linux / 12 BSD entries cover
new core-query JSON policy, MFL schema/assembly/event flags/JSON serialization,
52 desktop policy cases, module queue order, actual core-query/request/settings/
legacy-stream sessions, desktop baseline and Linux interaction servicing.

Pure core tests sweep allocation failures and verify cleanup, parser/length/
page limits, stale revisions, exact epochs and static/dynamic cache values.
Independent X/configuration observers check RandR geometry/output IDs, desk/
viewport/EWMH state, literal configured actions, paging and stale/unbind behavior,
size refusal, read-only queries, cancellation and late-result isolation.
The small core-query X fixture takes about 0.13–1.2 seconds on development/
emulated profiles. Existing absolute-deadline negative tests remain most of the
settings/request runtime. No performance benchmark or speedup is claimed.

The readiness repair retains **74 unchanged entries** by input and actual
core/MFL identity, and runs **39 correction or previously missing entries**.
Linux AMD64 Clang finishes its unrun full selected profile. OpenBSD ARM64 restores
its verified current compiled cache and repeats only the two settings entries;
its ten other current passes remain retained. The interrupted original OpenBSD
AMD64 archive is incomplete and supplies no acceptance: a fresh approved 8.0 job
completes all twelve selected checks. This is one composed affected checkpoint,
not nine full test suites or a repeat of every passing matrix entry.

Five scoped guest Clang ASan/UBSan/LSan exercises pass with leak detection enabled,
no suppressions and no diagnostics: core-query JSON/native request policy and
three actual-X fixtures with instrumented MFL and current uninstrumented core.
Four unchanged exercises remain retained; only the affected settings fixture
reruns with the readiness correction. Its instrumented module and core retain
exact identities. These emulated results do not establish native-CPU sanitizer
or complete release qualification. Existing Form font-exit cleanup remains
CLEAN-01 / Later and is not requalified.

Portable upkeep passes 177 cases with 14 explicit native-tool skips. The changed
MFL manual renders/formats; mandoc remains status 4 for inherited mso/LINKSTYLE/
section-spacing conventions, with one added subsection-spacing warning. No
clean manual-lint claim. Repository/link/diff and local review pass. Existing
legacy core compiler and OpenBSD unsafe-libc linker diagnostics remain explicit;
new query formatters add no diagnosed compiler warning. No new visual UI is
selected, so no screenshot is claimed.

All canonical captures validate gzip EOF/CRC, every payload and modes, with
source/profile/binary receipts. Only completed guest captures/duplicate archives
are retired after exact canonical comparison. A historical 496-file canonical
duplicate retirement restores about 187 MB after the Linux free-space guard
refuses to start. Host evidence, sources, incremental builds, install stages,
performance references and VM disks remain preserved.

## Failures and remaining gates

Original failures remain scoped in the receipt/private logs: initial Meson target
selection/registration; the X work-area cache comparison and interactive-placement
fixture timeout; Linux storage refusal; the stale OpenBSD private helper handoff;
three reproduced settings-startup failures; and incomplete original OpenBSD
AMD64 collection. The fixture selects deterministic placement and stops only its
identified orphan processes. The verified initial OpenBSD handoff is completed;
failed collection is never promoted to a pass. The settings race is fixed and
affected checks pass, rather than relaxing the override assertions.

This third PR closes the accumulated PR154-reference cycle: two PRs merged and
this combined candidate qualified/in review. On merge, use its canonical commit
as the next integrated reference and reset the cycle count to 0/3. Complete
install/default-X/full compiler-feature/milestone/release gates remain at their
required boundary; no installed-path or server-dependent behavior changes require
an early gate here. Deferred gates are not successful execution. Native CPU,
physical hotplug/hardware, toolkits/compositors and wider standards acceptance
remain separate. No GitHub Actions or status publishers are added.

IPC-02 remains open for other providers, menu/placement bindings, domain actions
and progress/decisions/cancellation, service adoption, observable shared-core
window/theme generations and CFG-01 preview/recovery. Topology epochs/binding
revisions do not supply that broader recovery contract. Performance remains
independent: PR154 reference, 2/5 merged, 3/5 after this merge; no comparison due.
