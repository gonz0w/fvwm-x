# Live-X outer-termination cleanup and Form successor focus

2026-10-08. Locally qualified at the combined five-story checkpoint. Source1374:
`48ec0ae384bb55dacf9c29aeef1226b3a87d6b1f89959e0904ee74a5ef36dc39`.
This adds one verification-infrastructure story to the four
[Form input stories](VERIFICATION-2026-10-08-FORM-INPUT.md). Production WM code
is unchanged by the harness story; the Form manual documents continuing input
and refusal of incomplete quoted timeout arguments.

An ordinary outer SIGTERM baseline leaves a privately owned stubborn child alive:
the observer retains actor status -15 and safely retires its own child. This
matches the ownership gap seen after earlier full-checkpoint timeouts, without
establishing the cause of those individual timeouts. Main-thread HarnessRuntime
now temporarily owns SIGTERM so the existing scenario cleanup can unwind, closes
owned processes in reverse order with their existing finite deadlines, and
restores the previous signal handler after the final runtime closes. Repeated
TERM requests during teardown coalesce. Nested runtimes can close out of order.
Caller replacement handlers are preserved. No core event-loop ordering changes.

Final Linux ARM64 normal and scoped runs pass seven C policy entries, two Python
harness entries and nineteen actual process/core entries per profile. Leak checks
are enabled for C policy and disabled for actual process/core; Python itself is
not sanitizer-instrumented. FreeBSD/OpenBSD ARM64 native focused overlays pass
Form policy, transport/input, harness self-test and owned child teardown.
macOS also passes the owned child teardown control. Complete 143/55/54-file
Linux/FreeBSD/OpenBSD archives verify payload hashes, modes, EOF, all 1374 frozen
developer source identities, eight consumed BSD source identities, exact runners
and recorded input/timer/termination outcomes.

The initial OpenBSD observer called getpgid across sessions and received EPERM;
the exact prior source, failure logs, complete native payloads and owned stub
cleanup record remain under initial-openbsd-observer-session-query. This is a
fixture portability failure. The revised child reports its own process and group
IDs; failure cleanup addresses only that owned stub. OpenBSD documents the
[cross-session restriction](https://man.openbsd.org/getpgid.2).
The first validator also included Python bytecode in its source-only check; all
payloads remain checked, and source identity checks now select the exact eight
frozen consumed files.

The Form man page is generated with the existing Linux Asciidoctor, then rendered
and checked with FreeBSD mandoc. Both baseline/candidate retain the same eighteen
existing diagnostics and mandoc status 3; the new text appears in the rendered
candidate. This is a focused paragraph check, not a clean whole-manual lint gate.
The initial FreeBSD renderer-unavailable attempt remains retained; no package is
added. Source and roff identities, generated text, logs and executed runner are
retained under manual-check.

Artifacts are in builddir/codex-2026-10-08/harness-teardown-production, outside
frozen module-ui1370 qualification. The combined successor owns ten paths and five
stories; no component-only full matrix has been dispatched. Further native
Linux release CI, physical hardware, toolkit and compositor evidence is separate.
Thread-created runtimes, foreign C signal handlers, hard SIGKILL and descendants
of an already exited process are not covered by this cleanup claim. Form partial
packet reads, shell callbacks and remaining parser/geometry owners stay open.

Ten combined runners are syntax-preflighted for the five-story successor. Required
future totals: 1520 standard entries/eight profiles, 120 staged identities, 192 BSD
installed/default-X invocations (48 each), optional 195/175, and 81 scoped checks
(29 C policy leak-enabled; 52 process/core leak-disabled). The two Python harness
checks remain distinct focused evidence. Dispatch requires strict predecessor
qualification; these counts are requirements, not completed wider coverage.

The five-story combined matrix is now dispatched after strict module-ui1370
qualification: ARM64 standard/compiler/install/default-X and scoped sanitizer
gates run first. AMD64 and optional profiles follow their owning roots. Ordinary
deadlines and the existing finite OpenBSD AMD64 TCG harness profile are unchanged.

Progress: scoped81 (29 C policy/52 process-core) passes. Linux ARM64 GCC196 and
both BSD ARM64 standard184 pass; FreeBSD ARM64 install/default-X48 completes.
OpenBSD ARM64 again exhausts its root partition during expected-installed-byte
copying after suite/install success. Its 103-file current tree is hash/mode
verified on /home; the initial attempt is archived before only unfinished
install/default-X work resumes. AMD64 and Linux ARM64 Clang remain running.
The OpenBSD creation runner now places future current evidence on /home at
checkpoint creation. No production source or deadline change is made.

Final combined qualification passes strict validation: 1520 standard entries
across eight compiler profiles, 120 installed code identities, 192 BSD default-X
driver invocations (48 each), optional 195/175 and 81 scoped checks (29 C policy
leak-enabled; 52 actual process/core leak-disabled). Seven complete final archives
validate the frozen source, modes, registered tests, installed bytes and EOF.
Evidence is in builddir/codex-2026-10-08/module-form-production.

The original OpenBSD ARM64 capacity attempt is retained before verified evidence
relocation and unfinished install/default-X completion. The optional mixed-cursor
profile retains one menu-release timeout, three unchanged affected passes and the
exact third replacement; its other 194 results remain intact. The timeout cause
is unestablished and remains a verification follow-up. No production code or
deadline changed during qualification. Native release CI and physical checks
remain separate. The next argument/data stories use a different frozen source.
