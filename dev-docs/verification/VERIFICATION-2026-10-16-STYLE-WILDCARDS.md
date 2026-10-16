# Style wildcard matching — 2026-10-16

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38), following merged
[PR #102](https://github.com/gonz0w/fvwm-x/pull/102). The three-item checkpoint is
locally qualified through the composed source scopes below. PR publication
awaits the repository-visibility decision; no release is authorized. VESA #93
cause investigation remains deferred.

## Three substantive improvements

1. Prepare the minimum byte length of unescaped wildcard selectors and reject
   shorter values before entering the general matcher.
2. Prepare the anchored literal suffix after the final wildcard and reject a
   mismatching suffix after checking the subtraction bound.
3. Resolve exactly one unescaped star with no question mark from prefix/suffix
   equality and the non-overlap bound, including successful matches.

The final review correction preserves constant-time literal-prefix/trailing-star
and all-star handling: neither performs a new full-value length scan. Other
eligible patterns compute a needed field's byte length lazily, at most once in
one synchronous lookup/mark operation. Literal, universal, numeric and
prefix-rejected rules require no new length scans. Fresh subjects invalidate
lengths after identity changes; no pointer survives an operation.

Selector IDs remain immutable after publication. Existing adjacent merges retain
the destination ID, full record moves copy pointer-free metadata, and UseStyle
copies presentation without replacing selector identity. Escapes and remaining
question/multi-star cases retain the original wildcard owner, including its
multi-star/empty-value behavior. No configuration syntax, rule precedence,
update/grab boundary, X publication, resource generation or native packet changes.

On these 64-bit targets metadata grows from 40 to 144 bytes per style and the
operation-local subject from 48 to 80 bytes. Preparation adds linear work;
general successful patterns can pay necessary checks plus the old matcher.
Call reductions describe named workloads, not a universal elapsed speedup or a
bound on all recursive wildcard work. Broader matching indexes, repeated-ID
simplification, update planning, grabs and state coalescing remain separate.

## Sources and cadence

| Scope | Exact input and result |
| --- | --- |
| One broad checkpoint | Signed runtime `42e1233f7cf8670adda8aa142444d61b76e49328`, tree `b2ca9acbfbb123950ede64e83c796fe4cdafa11a`; 1974-file archive `df971c0186dd2e2ef13b27ff8f4bb26877221194f0ead7af5a263573b76f562a`. Only the frozen README differs from that signed tree: its 263-word draft was shortened to 246 words to satisfy the commit hook. Exact documentation-only hashes are retained; every runtime/build/test byte matches. |
| Reviewed C/test correction | Signed `ee180b0684f74c10984ddec75a43099987211139`, tree `af7b675f19bb5c681aefd9770dc30f9ad06a452d`; 1975-file archive `6e632b9cc15dfe3ca1b5979b2823e99310396270ccf2e0c12ca768e377f174dd`. Adds the no-scan shortcut and portable test counters, with affected native checks. |
| Final test-only correction | Signed `97bc07ed564e040f4f9fb874b5023c21a50f2cc4`; 1975-file archive `b7e9dd1357511816b0895bbd7be9e172cfc787cb080ea0c9c4eb8755233dd202`. Only `tests/live_x/style_wildcard_live.py` differs from the preceding archive. Every compiled input remains identical; the external client now uses automatic placement and bounded supervision. |

Implement related work with one incremental Linux development profile, then run
one broad matrix. Review corrections receive affected checks, not another full
matrix. The final continuation proves byte identity for 18 completed corrected
native results and six installs; it reuses them, runs the twelve unfinished
native entries and three sanitizer entries, and completes ten real-client probes.
Maximum two full qualification jobs run concurrently. Boot/recovery preparation
and small isolated controls overlapped them. No broad suite is rerun for a PR,
documentation or an already understood unrelated failure.

The [machine receipt](VERIFICATION-2026-10-16-STYLE-WILDCARDS-COMBINED.json)
binds source bytes/modes, commands, profile options, original failures, preserved
archives, continuation proofs, installed identities and remaining release scope.
Owner raw evidence remains under
`builddir/codex-2026-10-16/style-wildcard-batch`.

## Results

The original full runs execute **1992 standard entries: 1988 pass and four new
BSD test-counter assertions fail**. All **437 optional entries pass**. No row is
replaced or relabeled. All other affected/retained entries pass every profile:
`style-id-filter`, `style-profile-xvfb`, `window-update-xvfb`, `ewmh-state-policy`,
`ewmh-state-xvfb`, `ewmh-inventory-xvfb` and `atom-batch-xvfb`.

The final affected scope passes **30 native policy/core entries**, **three scoped
ASan/UBSan entries**, and **ten staged real-client invocations / 70 apply-remove
cases**. It verifies **ten final stages: 150 code and 99 catalog identities**.
Four final authenticated default BSD displays pass the new client probe and
previously unperformed staged geometry at 100% and 150%: **eight invocations /
56 cases**. The initial four default displays pass twelve core probes; twelve
initial Linux staged geometry invocations / 84 cases remain in their original
source scope. The final pure shortcut changes no X/server API or ordering owner;
unchanged owners retain that broader evidence.

| Profile | Original full pass/fail | Final affected entries | Final stage / real client |
| --- | --- | --- | --- |
| freebsd-amd64/default | 242/1 | 3/0 | verified / 7 cases |
| freebsd-arm64/default | 242/1 | 3/0 | verified / 7 cases |
| fvwm-x-linux-arm64-svg/clang | 255/0 | 3/0 | verified / 7 cases |
| fvwm-x-linux-arm64-svg/gcc | 255/0 | 3/0 | verified / 7 cases |
| fvwm-x-linux-arm64-svg/minimal | 203/0 | 3/0 | verified / 7 cases |
| fvwm-x-linux-arm64-svg/software | 234/0 | 3/0 | verified / 7 cases |
| linux-amd64/clang | 255/0 | 3/0 | verified / 7 cases |
| linux-amd64/gcc | 255/0 | 3/0 | verified / 7 cases |
| openbsd-amd64/default | 242/1 | 3/0 | verified / 7 cases |
| openbsd-arm64/default | 242/1 | 3/0 | verified / 7 cases |

Against the six original bound Linux stages, six WM identities change and 139
identities remain identical. The four newly enabled BSD stages have 104
previously unbound installed identities; they are **uncompared**, not called
changed or unchanged. Final non-WM tested build bytes remain identical within
each refinement phase. Rpath-adjusted expected bytes equal installed bytes;
staged version checks and actual staged WM/MFL consumer checks pass.

The original retained state coverage includes fourteen invocations / 28 fresh
normal/missing modes, 336 client-list rows, 168 work-area rows, 56 atom rows and
28000 strut cases. Those policy fixtures include repeated deterministic seeds;
they are not independent mutations. Initial style probes contribute 330 workload,
90 legacy-cost and 90 new wildcard-cost rows; final affected probes contribute
242 workload and 66/66 cost rows. Their instrumented elapsed values are
observations, not byte-identical predecessor timing.

Portable `make verify` runs once: 166 passes / eleven explicit native-Meson skips.
Final test-only changes use Python compilation and repository/whitespace checks;
no portable or native full suite is repeated for documentation. Compiler
warnings from unmodified legacy sites and configuration profiles remain captured;
there is no diagnostic in the changed C files, and no warning-free or advisory-
scan claim is made.

## What the controls establish

Policy compares 699776 exhaustive short-pattern/selector/subject combinations
and 100000 seeded varied-field cases with the original predicate. Long-value,
non-overlap, NULL, escape and fallback controls retain byte semantics. Three
paired actual-core 64-rule workloads reduce general caller invocations from 64
to zero for minimum rejection, suffix rejection and successful single-star
lookup. Rule creation, precedence, removal, UseStyle/merge ownership and explicit/
outer-loop update barriers execute through the actual core.

Review reproduced an avoidable full scan of a 16-KiB value for `probe-*` in the
first candidate. The corrected unit proves zero length scans for that prefix
case and all-star patterns while preserving their results. The final tests count
**outer caller invocations** explicitly: some compiler/linker choices route the
wildcard owner's recursive calls through a wrapper too.

All four original BSD failures are precisely the new `calls == 1` assertion in
`wildcard_work_counts`, not a differing match result. On OpenBSD AMD64 an isolated
trace observes 17 recursive wrapper entries where one caller was intended.
Changing only counter depth makes the whole unit pass with the production
matcher unchanged. Original raw failures, control sources and commands remain
bound. Final corrected units pass all ten profiles before the blocked BSD
installs are enabled; no full suite is rerun to erase those failures.

The first external real-client fixture omitted automatic placement and hung.
Its six failed attempts and exact partial sources, results and installed bytes
are preserved. The corrected fixture passes against the unchanged staged WM.
A parent now owns the server/WM/module/client processes, with all potentially
blocking Xlib calls in a supervised worker. Final probes require successful
worker exit, a live core before teardown and clean child exits. They check
quoted literals, minimum/suffix rejection, successful single-star matching,
question/multi-star fallbacks and overlapping-span rejection through commands,
EWMH extents and actual frame/client geometry. The 18 passing compiled checks
are retained across this Python-only correction rather than repeated.

## Environment recovery and limits

The first dispatch encounters three offline AMD64 VM endpoints before transfer;
Linux ARM64's active run is retained. Saved hardware arguments restart the owned
VMs. Both BSD AMD64 boots expose filesystem inconsistencies before any WM test.
Manual recovery uses disposable overlays and preserves the original failed-boot
state and previous source/build trees. Read-only filesystem checks are clean,
normal boot/SSH is verified, and all frozen source bytes/modes are reconstructed
in fresh build trees. OpenBSD's new current tree lives on `/home`. This is
verification-environment recovery, not VESA/driver cause investigation.

FreeBSD's optional package-file checksum observation exceeds its 180-second
coordinator limit, continues in the guest and later records completion without
checksum error text. Its exit status was not observed and it is not labeled a
passing gate or repeated. It checks installed package files, not base/private
files or advisories. Dependency/build/runtime identity evidence has its own
recorded scope. OpenBSD AMD64 preparation uses a bounded 600-second configure
limit after the preceding checkpoint's known 300-second preparation interruption;
other configure limits remain 300 seconds. Existing functional deadlines are
unchanged. Failed setup, test and fixture attempts remain separate evidence.

These are native binaries in the owned Linux/FreeBSD/OpenBSD guest environments,
not native-CPU, physical GPU/input/toolkit/compositor or release qualification.
Scoped Linux ARM64 sanitizer executes address/undefined checks: leak detection is
on for the pure unit and off for `_exit` core fixtures. Initial and final scoped
runs are separate candidate executions, not a general leak audit. Prior broader
readiness/helper failures remain historical observations; their absence here
does not prove a cause or close VERIFY-01 / CI-01. VESA #93 stays deferred.
