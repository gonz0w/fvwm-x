# XDG installed acceptance — 2026-10-29

This checkpoint finishes #56's frozen installed interoperability work around
merged PR300 `dbcbe2f1cb1d57ee0a6c90f130b34502dffe66fd`. It changes verification
fixtures and documentation; production behavior is unchanged. The selected
[tracker contract](../PROJECT-TRACKER.md#r1-xdg-installed-acceptance--2026-10-29)
keeps broader release, UI, FileManager1 and service acceptance with their owners.

Qualification input closure
`9d14e135f6c3ac5317040fc45f648952ad6e0a8bc67d954334110a9c0c33a9ee`
contains the exact merged source plus the two independent fixture files, 2,985
regular inputs. The runtime sources are PR300; the fixture Python/C hashes are
`72d44c86c7a15e0aaed1c6134cdf1fd66084a2b49db504c60c0b75366ffdb63c` /
`1861d19407b6d2501d25af3350ae04cc766d3be44e915147183d4a29d3e314f0`.
Source guards bracket execution. Modified reporting prose is outside the executed
runtime input closure.

## Frozen criteria and evidence composition

| #56 criterion | Accepted component evidence | Remaining installed outcome |
| --- | --- | --- |
| Desktop Entry lookup/visibility/localization/actions and exact Exec grammar | [Applications adoption](VERIFICATION-2026-10-28-XDG-APPLICATIONS.md) and [menu parity](VERIFICATION-2026-10-28-XDG-MENU-PARITY.md), merged PR293/294 | Real installed provider/menu discovery and literal main/action execution supplement the accepted grammar tests. |
| Base-directory precedence, MIME roles and startup success/failure | [Directory application roles](VERIFICATION-2026-10-28-APPLICATION-ROLES.md), [selected-file roles](VERIFICATION-2026-10-28-SELECTED-FILE-APPLICATIONS.md), [provider resilience](VERIFICATION-2026-10-29-XDG-PROVIDERS.md), [startup activity](VERIFICATION-2026-10-28-STARTUP-NOTIFICATION.md), [group lifetime](VERIFICATION-2026-10-29-STARTUP-GROUPS.md) | Real workers, default precedence, native announcement/exec, failure/display absence and late group-ID observation on installed target profiles. |
| Shared inherited icon resolver handoff | [Named menu icons](VERIFICATION-2026-10-26-NAMED-MENU-ICONS.md) and PR293/294 consumer parity | Reuse accepted named/inherited/hicolor/scale/missing semantics. No second resolver or new image-rendering qualification. |
| Installed entry discovery/launch/roles without overwriting personal defaults | Earlier Linux installed/theme results retain exact scope | Complete focused Linux/FreeBSD/OpenBSD AMD64/ARM64 installed-XDG coverage below. |

This is an owner acceptance checkpoint, not a new full compiler/feature/default-X
release matrix. Existing semantic, three-theme UI, minimum-SDK/race and scoped
sanitizer results retain their original inputs and limits. No new product UI,
screenshot, dependency, performance tuning or bus integration is introduced.

## Focused target results

| Target | Execution | Installed groups | Result |
| --- | --- | ---: | --- |
| Linux ARM64 | Owned Linux kernel/container, native ARM64 CPU in VM | 5 | Passed |
| Linux AMD64 | Owned Linux kernel, QEMU TCG AMD64 CPU emulation | 5 | Passed |
| FreeBSD 15.1 ARM64 | Owned native guest kernel, ARM64 CPU via HVF | 5 | Passed; final provenance proof composed separately |
| FreeBSD 15.1 AMD64 | Owned native guest kernel, QEMU TCG AMD64 CPU emulation | 5 | Passed |
| OpenBSD 8.0 ARM64 | Fresh verified release seed, ARM64 CPU via HVF | 5 | Passed |
| OpenBSD 8.0 AMD64 | Fresh verified release seed, QEMU TCG AMD64 CPU emulation | 5 | Passed |

The lead selects an actual private Meson prefix, not a relocated DESTDIR.
Current built core/MFL binaries run an explicit owned configuration; they are not
claimed as fully installed native modules. Existing Meson tags install the real
menu/browser, Desktop controller/worker, helper and desktop metadata. The menu
must find the helper through its linked private module directory: no adjacent
test helper, override or build-tree fallback conceals path errors.

Each profile executes five independent groups:

1. Installed identities, executable modes and exact shipped Desktop Entry bytes;
   genuine installed-provider discovery finds the browser entry and its flags.
2. Desktop provider/menu discovery, high-root masking and actual main/action
   launches. Fixed catalog references are parsed without evaluating generated
   FVWM text. Menu/helper/application exec preserves the directly owned child
   PID; exact argument bytes/cwd, fresh startup IDs, mapped XID observations and
   real application Wait0 are required.
3. Real browser-worker scan, directory/file roles and raw path location, plus
   the installed `--inspect --reveal` CLI for actual URI decoding without X UI.
   Empty providers, malformed preferred entries, same-ID masking, missing
   preferred entries and exact-MIME versus broader defaults use genuine files
   and the real file(1) classifier. No worker replies are synthesized.
4. Known missing executable produces a real nonzero exit, stderr and
   `protocol-ended`, distinct from mapping/success. No-DISPLAY fallback runs the
   ordinary app with inherited startup ID cleared and a direct Wait0.
5. The late-group oracle first requires a unique legacy-class observation and
   actual M_MAP. A different classless ID must be pending before changing only
   the unmanaged leader; it then resolves to the exact dependent. Last-reference
   release and producer Wait0 complete the probe. PR300's exhaustive ownership
   and independent real expiry results remain separate; no TTL is repeated.

Every installed consumer call snapshots synthetic preference bytes, mode and
mtime before/after and requires equality. The fixture deliberately rewrites its
own fake MIME settings between provider subcases; therefore initial/final
case-wide preference hashes differ by design. This is per-consumer preservation,
not a claim that fixture setup never changes files. Real user defaults are outside
the isolated HOME/XDG roots and are not used by these probes.

All applications/workers/producers are directly owned. The core owns its native
module proxy; that proxy records its actual MFL child Wait and whether fallback
termination was needed. Do not claim Python waits for a core-owned proxy PID.
Normal core Quit and empty cleanup-failure lists are required. Forced child
cleanup fails the fixture. The X server uses ordinary owned-session teardown.

## Installed artifact provenance and retained failures

Four Go outputs retain direct build/installed byte equality. The native helper
is also checked byte-for-byte when no install edit occurs. On BSD, Meson normally
removes build RUNPATH entries; comparing the raw build file to installed bytes
was an invalid cross-platform provenance oracle. The original FreeBSD/OpenBSD
failures and successful build/install commands remain recorded.

The corrected proof copies only the built native helper into private scratch
storage. It reads the same target Meson's current owned install metadata, binds
source/build/prefix/version, requires `strip=False`, and applies the exact target
Meson ELF fixup on that copy. Full normalized bytes/SHA256 must equal the actual
installed helper; the original binary and metadata remain unchanged. Only the
inspected Meson 1.10.2 and 1.12.1 signatures are supported; the latter supplies
its recorded target system. Unknown transforms/API layouts fail. This is
provenance evidence; actual helper execution is the separate functional oracle.

FreeBSD ARM64's runtime pass precedes the final stricter metadata check. The
later result is explicitly `passed-provenance-only`: it adds no runtime cases.
Composition requires identical source closure, paths, installed binary hashes,
sizes, modes and desktop metadata across those receipts. The final proof binds
the selected source directory and inspected version/signature pair.

The initial Linux fixture expected only `fvwm-file-browser`, while the shipped
Desktop Entry uses `fvwm-file-browser --reuse -- %u`. With no URI, only `%u`
disappears. The corrected independent expectation preserves `--reuse --`, the
one-entry assertion, exact metadata and StartupNotify=false. This one assertion
was the only fixture-source correction; runtime code was not changed.

The unrelated capture executable also receives an ordinary Meson PNG RUNPATH
edit. It is built/installed by the shared tag, but is outside #56's launch
artifact equality and behavioral claims. Full installed graphics/PNG closure
retains its existing owner. Neither install fixup is called a product regression
or hidden by weaker launch assertions.

## Receipt and cleanup

The [compact machine-readable receipt](VERIFICATION-2026-10-29-XDG-INSTALLED.json)
records all six source/profile/install identities, observed main/action handoffs,
role probes, actual terminal results, archive hashes and canonical helper proofs.
Thirty runtime groups and 60 worker results pass; the FreeBSD proof-only check
adds zero runtime cases. Full polling/command output remains in the verified
local archives identified there. Fixture runtimes are 0.342/4.475 seconds on
Linux ARM64/AMD64, 1.373/5.600 on FreeBSD and 2.029/13.157 on OpenBSD; these
are qualification durations, not comparative WM performance measurements.

All owned labs stop normally. Both completed OpenBSD job keys and disposable
disks are retired after archive and critical artifact verification, recovering
8,363,769,856 allocated bytes; the earlier failed disposable disk was also
retired. Current reusable target builds/private installs and verified evidence
remain. Older guest log/archive duplicates retire only after complete host
verification. Other owners' source, disks and performance references remain.

The source-only implementer, analyst, workflow inventory and separate reviewer
roles were available; actual backend model/effort metadata is unverified.
Final staged source/evidence review is recorded with the signed PR handoff.
No native check is repeated merely to update prose or open the PR.

## Boundaries and continuation

These new receipts do not show a helper-absence case; earlier Linux helper-
absence evidence remains source-bound to its original checkpoint. BSD detached
browser-application reaping, browser UI/theme behavior, full installed core,
BSD default servers, optional feature/compiler profiles, physical hardware and
release acceptance are not inferred from these focused checks. FileManager1
remains #67/#77 work. Autostart remains #72-owned. #257/#258 stay parked.

No performance comparison is due. Merged PR300 is 1/5 from the accepted PR299
reference; a qualification-only change does not advance that runtime counter.
Retain current reusable profiles/private installs and compact current evidence;
retire only verified duplicates and unused disposable disks. Final source/evidence
review and signed author-DCO handoff precede #56 closure.
