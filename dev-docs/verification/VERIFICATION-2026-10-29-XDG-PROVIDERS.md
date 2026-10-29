# Application provider resilience — 2026-10-29

One malformed `.desktop` file could abort every directory/file application
choice. This batch keeps valid different handlers usable, preserves the rejected
entry's desktop-ID mask, skips narrowly defined optional-input permission
refusals, and reports bounded quoted warnings through existing worker stderr.
Personal MIME preferences and fresh literal launch validation remain unchanged.
No new UI, dependency, resident or provider wire schema is introduced.

The [machine receipt](VERIFICATION-2026-10-29-XDG-PROVIDERS.json) binds exact
source, commands, native results and limits. Base is merged PR298
`86d9ffd02c44e57746d603d74287afa772d73382`. The 698-file source02 closure is
`cc434d9a6ffe661f2c139d95aacd13e412310c6f9295dc7627fb69ad58abda08`;
its nine-file delta includes seven Go files, the independent fixture and manual.

## Outcomes and source boundary

Both existing role consumers share the same effective desktop inventory. Parse
or metadata rejection now records an ineligible first-root owner, diagnoses it
and continues to different IDs. A lower same-ID file is never exposed by this
refusal. Existing specificity/default ordering and fresh selected-handler
validation remain in place.

Optional mimeapps, aliases and subclasses skip only a same-path open-stage
missing-file or permission error. Missing files are quiet. Permission refusals
warn; source/work limits, cancellation, special files, post-open failures,
malformed preferences/graphs and application-directory/entry acquisition errors
remain fatal. This is a narrow policy, not a claim to tolerate every XDG read
failure. Diagnostics are operation-local: 16 details plus one omitted-count
summary, 512-byte paths and 256-byte messages, quoted on one stderr line.

The expected behavior follows [MIME Applications section4](https://specifications.freedesktop.org/mime-apps/latest/default.html)
and [Base Directory section4](https://specifications.freedesktop.org/basedir/latest/).
The former malformed-preferred abort oracle changes deliberately after a real
old-provider regression establishes that valid different defaults were blocked.

## Focused verification

| Check | Result |
| --- | --- |
| Before implementation | 107 unchanged baseline files verified; eight fallback/masking/unrelated-invalid cases fail on the old provider. |
| Normal Go | 263 passing records across two affected packages: 172 browser records retained from source01; 91 source records on source02. No skips. |
| Race / minimum SDK | 263 passing records/two packages in each run, no failures/skips; Go 1.27.2 and floor 1.26.9. Vet passes. |
| Real permissions |Four cases execute as UID 65534 and first prove actual `fs.ErrPermission`; no root skip or injected permission result. |
| Build/install |One cached Meson source-inventory refresh, one affected browser build, and file-browser tag install into the existing stage pass. Built/installed hashes match. Core/native startup helper hashes remain unchanged. |
| Installed worker | 17 real requests/nine groups: absent and malformed providers, ID masking, config/data precedence, MIME specificity, bounded diagnostics, fatal malformed preferences and unchanged preference hashes/modes/mtimes. |
| Actual applications |Two recorder applications observe literal/raw0xff arguments and exact cwd, then exit and receive actual Wait0. Worker submission alone is not treated as application success. |
| Documentation |Affected man page generates on Linux and renders with host mandoc at 80 columns. Repository/link/whitespace gates follow final documentation freeze. |

The affected runtime commands total about 23 seconds, including the failed initial
normal run. This is command time on the recorded lab, not whole-task development
time or a controlled workflow-speed comparison. Native Linux ARM64 runs inside
the existing virtualized lab. No new C/sanitizer, X/theme, BSD or full matrix run
is justified by the unchanged UI/native code and portable error handling; older
accepted results keep their original source/profile limits.

## Failures and review

The first normal checkpoint caught a permission-test setup error: its UID 65534
child inherited private root `GOTMPDIR`, despite `TMPDIR=/tmp`. The child-only
correction sets both; all EACCES and provider expectations remain intact. Retain
the passing browser normal result, rerun the affected source package, then run
only the checks not yet completed. Production and fixture sources stay identical.
The bounded test child has a 30-second context and 20-second Go test deadline.

Separate source/fixture review found no actionable findings. Final source,
evidence and exact-tree confirmation are recorded with the signed PR candidate.
Agent review does not supply human merge approval; backend model metadata is
unverified.

## Retention and remaining work

PR297 and PR298 are merged; canonical trees, GitHub signatures, actual-author DCO
and branch/Project cleanup are verified. The missing local GitHub GPG key and
initial attempt to delete already-retired remote refs remain explicit in the
receipt; no trust settings changed.

The new retention rule retires nine superseded owned style captures, recovering
946,151,424 guest bytes, and two obsolete host archives totaling 136,251,453 file
bytes. Current builds/SDK caches, stage, private test-provider cache and the
`m5-performance-xdg-20261028` baseline remain. The real file 5.45 test provider is
reused from the same checksum-pinned package inputs, extracted privately without
system package installation; this is not a package vulnerability assessment.

All 33 current evidence files pass host archive/member/payload verification;
archive SHA256 is `3fd9dffaaa64106331ed9d0c4f7f74b8c3c0641b431fa977c3991d519c7596a6`
(161,929 bytes). Retire 32 duplicate guest logs, leaving the latest source guard
and small continuation receipt. Both the owned container and VM stop normally.
Keep this current-review evidence only while relevant; compact Git results and
published screenshots remain after older raw data is retired.

#56 remains open for late unmanaged group-leader property observation and full
installed-target acceptance. That core checkpoint needs bounded leader/client
tracking, raw-XID property/destroy dispatch and coordinated event-mask ownership
at management, override-redirect, tray and teardown transitions. A saved-mask
OR/restore or polling workaround is not adopted. #67 retains optional FileManager1
under #77 plus its broader lifecycle/target acceptance. #257/#258 stay parked.
Performance remains 4/5 after PR297; this runtime merge reaches 5/5 and makes the
next comparison due. Documentation-only PR298 does not advance the counter.
