# Optional FileManager1 adapter — 2026-10-31

Owner: Cam; #67/#77; `codex/r1-filemanager-bus`, based on merged PR314
`92742f5d5c2ed300fdb1b1a22e8f1a234a07e9d0`. This completes the
[selected eight-outcome feature checkpoint](../decisions/FILEMANAGER-BUS-2026-10-31.md),
not whole-issue or release qualification. The
[companion receipt](VERIFICATION-2026-10-31-FILEMANAGER-BUS.json) preserves exact
source closures, commands, artifacts, failures and limits.

The explicit browser service exports all three FileManager1 methods through the
existing optional native libdbus helper. URI planning remains in the Go browser:
decode once, retain filename bytes, normalize duplicates and group lexical
parents. Admission reserves bounded capacity, admits groups in order under one
deadline and retains started children until actual Wait. Unknown outcomes stop
remaining groups without replay or terminating admitted independent views.
No default-role takeover, activation file, new settings store or mandatory bus
dependency is added. The [manual](../../doc/fvwm-file-browser.adoc#filemanager1-service)
and [source configuration example](../../doc/examples/FvwmFileManager1.fvwm)
describe opt-in setup, admission semantics and recovery.

## Executed qualification

| Profile | Observed result and boundary |
| --- | --- |
| Linux ARM64, native CPU under AppleHV | Enabled/disabled affected helper and browser builds, pure C policy, six affected Go packages, final browser race/vet, installed helper/browser/manual byte equality pass. |
| Real private D-Bus, Linux | Eleven raw native bridge groups pass normally and under scoped ASan/UBSan with leak checking; no sanitizer reports. This injects private parent replies and is separate from real browser admission. |
| Linux Xvfb, actual WM/MFL/Form | All twelve recorded groups pass in Slate dark, Dune light and enlarged Classic. Installed browser/helper use build-tree core, MFL, Form, Desktop, worker and capture peers. This is not a wholly installed desktop. |
| FreeBSD 15.1 ARM64, native CPU under AppleHV | Source02 affected C/Go builds, C policy, actual browser process/descriptor tests and no-bus refusal pass. The later platform-neutral two-file Go serialization delta has final Linux coverage only; no final BSD delta or BSD X claim. |
| Manual | Final generated/installed roff matches byte-for-byte; native macOS mandoc rendering exits0. Lint exits4 and is not a pass. |

Final browser ordinary and race runs each record192 passing cases/subcases.
The five unchanged companion packages contribute115 passing records from the
combined source01 run:307 composed ordinary passes across six packages.
FreeBSD records187 browser passes at source02. These are test records, not
independent end-to-end scenarios. Valid unaffected results are reused; there is
no fresh six-target matrix solely for this PR.

The actual-X checks cover all three empty-return methods, genuine same-provider
and same-window reuse, grouped selection and observed properties, encoded
non-UTF-8 names, literal metacharacters without target execution, duplicate
normalization, malformed signatures/URI/limits, competing ownership, absent bus
and disabled helper. Real MFL replacement and core Restart retire old transports
and browsers while retaining the service's bus owner. Exact socket peer PIDs,
native module Wait receipts and fresh handshakes fence the new session. New
providers appear only after explicit fresh requests. Service exit subsequently
preserves independent browsers and the borrowed bus. Browser recovery is not
automatic.

Raw bridge checks cover status mapping, stale/future IDs, partial matching
replies, a refused second pending call, malformed/duplicate/oversize replies,
an observed full4096-byte producer pipe, admission expiry, late replies, both
pipe losses and actual bus loss. Expiry was observed at about2.009 seconds in
the ordinary run with a750 ms observer scheduling allowance. Timeout error
delivery is best effort; connection loss/unknown outcome is allowed. UNIX-FD
injection is unavailable with dbus-send: native FD bounds and rejection were
source-reviewed, without a dynamic FD-rejection claim.

## Sources, failures and corrections

Final runtime/fixture source05 closure is
`1373110b1dd709e9b7636c4268ff817627ada6acb30a6475c7e2866ca9598b60`.
The final X fixture is `046116986f16c83c57ed2ede4d325f011366b47c9f5636cd332d3f0bf28db0b1`.
Final browser source includes the serialized-admission correction; C bridge,
policy and build inputs are unchanged from their passing native/fault/sanitizer
checks. Later gallery, status, evidence and example-comment changes do not alter
the tested runtime. The receipt retains each earlier closure separately.

Source review found that ScanLines could admit complete JSON without a terminal
newline at EOF. A bounded delimiter-required reader and exact-boundary regression
replace it. The first race run also exposed the race runtime's default one-second
exit sleep exceeding existing500 ms worker tests. The documented race profile
uses `GORACE=atexit_sleep_ms=0`; production and those test deadlines were unchanged.

The first X fixture used incorrect standard Peer/Introspectable interface names.
The next failed to observe a proxied MFL: the core had already started its real
MFL before configuration, and a duplicate lost the socket lock. The corrected
fixture explicitly replaces that instance and checks actual peer credentials.
The opt-in example likewise avoids starting a duplicate MFL.

The subsequent grouped request exposed three resident browsers instead of two.
Both sibling reuse clients could query the same single-slot provider before
checking its folder; the resulting busy fallback explains the extra view. The
precise socket interleaving was not captured and remains a source-backed
inference. Serial group admission removes that self-contention. An actual
held-receipt process regression proves that later groups remain unstarted until
admission and stay unstarted after unknown/cancelled outcomes; the final real-X
run retains the original folder provider and exactly two providers. No global
wire limit or existing reuse behavior was changed.

Two lead fault-run setup errors used the fixture outside its expected source
root; their import/path failures remain recorded. Canonical source-tree execution
passes. Failed aggregates remain failed even where earlier constituent commands
passed. The final roff lint has converter `.mso`/`.LINKSTYLE` errors, unsupported
table spacing, paragraph spacing and long-line diagnostics; rendering retains
the content. No clean-lint or baseline-equivalence claim is made.

## Captures, cost and remaining acceptance

Nine unmodified1440×1200 captures accompany the three passing workflows. The
[gallery and exact capture/configuration manifest](../../doc/development-preview.md#filemanager1-browser-requests--work-in-progress)
show grouped reveal, folder opening and properties, including enlarged text and
explicit classic colors. Selected captures are visually inspected; images alone
do not qualify input accessibility, assistive technology or hardware rendering.

The recorded Linux unstripped helper is257,656 bytes and browser6,540,615 bytes.
These are file sizes, not RSS, marginal feature cost or a performance comparison.
The disabled helper has no D-Bus linkage. Service processes run only after
explicit opt-in. The merged-checkpoint counter remains4/5 from the runnable
PR310 core reference; merging this runtime candidate reaches5/5 and makes the
next paired comparison due.

The startup ID remains advisory. Admission success does not establish filesystem
resolution, presentation, hard RSS or real-time bounds. Full supported-target
installed/service/selection coverage, clipboard/XDND and the remaining session
adapter/inventory/cost criteria stay with #67/#77 and release acceptance.
#257/#258 remain parked.

All954 files across eight retained raw archives verify against their manifests.
Obsolete guest checkpoint/input duplicates totaling34,503,818 bytes are retired;
current source, incremental builds, installed prefixes, SDKs/caches, latest
source05 evidence and the runnable performance reference remain. Four older
full host source archives totaling106,974,871 bytes are replaced by2,237,828
bytes of exact base/content-delta/mode records; reconstruction is verified. Owned Linux and
FreeBSD labs stopped normally. Separate `temper_reviewer` source/artifact/receipt/visual
review is clear with no unresolved findings at tree
`3412e8e1e708a3ff1509c664c52c5b5c5e296498`; the final review-status metadata
delta is inspected separately. Backend model/effort metadata is unverified.
This agent examination does not substitute for maintainer approval.
