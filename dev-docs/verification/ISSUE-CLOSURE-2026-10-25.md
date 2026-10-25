# Accepted cleanup scope reconciliation

The 2026-10-25 backlog snapshot has 69 open issues. PR206 is merged as
`856e9e4efb2a8439f714060fb2dc71273ab7a9cf`, with verified canonical signature,
author DCO and exact candidate tree; its completed branch is retired.
Recent M3 target groups implement components under open acceptance owners.
The next checkpoint targets complete #184/#49 outcomes and #47's distinct floor
inventory; it must not treat partial implementations as finished issues.

## DEAD-01 #35: completed

The named source-removal scope is exhausted by existing merged evidence:

- [Inactive-function checkpoint](VERIFICATION-2026-10-07-STACK-TRAVERSAL.md):
  seven proven inactive/unreferenced functions, 234 lines removed, active
  preprocessed bytes unchanged; exact 1314-file combined acceptance across eight
  compiler profiles, default BSD X, installs, optional and scoped sanitizer gates.
- [Later unbuilt-source review](VERIFICATION-2026-10-07-STACK-GROUP-CLEANUP.md#unbuilt-source-removal):
  empty fvwm/template.c and obsolete libs/asprintf.c removed. Six OS/architecture
  environments, including both Linux compiler profiles, qualify system
  asprintf/vasprintf and active link references. Keep required config declarations
  and active system formatter wrappers.
- [M1 disposition](../decisions/M1-RUNTIME-BASELINE-2026-10-23.md): the separately
  owned converter/runtime predecessor subset is already qualified in PR189.
  Broader framework/tooling/packaging retirement remains #40/#42/#52/#55.

The tracker instruction to review the removed template/fallback sources was
stale. No additional named source gap remains under #35. Reconcile its tracker,
issue acceptance/labels and Project status as completed. This closes the accepted
finite source scope, not release/native/hardware qualification or every future
source-cleanup opportunity. No code changes or repeated runtime testing is needed
for this status correction. Separate read-only scope review confirms the evidence;
requested model routing remains unverified where client metadata is unavailable.

## LIB-07 #93: not planned / superseded

The owner explicitly dropped OpenBSD 7.9 from support while selecting 8.0 and
future new releases. #93's exact failure is a pre-client VESA/DDC crash on 7.9;
its 8.0 note records no reproduced crash in the accepted PR136 baseline and
introduces no separate finite current-baseline defect. Close as **not planned**,
not completed/fixed. This deliberately supersedes the earlier historical/deferred
Backlog disposition for the unsupported release.

Keep both failed starts, exact original platform identity, diagnostic/control
reports and the two unfulfilled cause/fix acceptance criteria. No root-cause or
vendor/driver repair, vulnerability verdict or upstream notification is claimed.
[Baseline and owner scope](../decisions/OPENBSD-8.0-BASELINE-2026-10-20.md),
[accepted baseline evidence](VERIFICATION-2026-10-20-OPENBSD-80.md),
[original investigation](VERIFICATION-2026-10-14-BSD-X-STARTUP.md).
A reproduced failure on a supported release would need its actual new evidence;
this disposition does not waive current-baseline or release/hardware checks.

## Remaining outcomes

#184/#49 require their combined platform/profile/install and dependency acceptance;
#47 requires an actual build/runtime/developer dependency-floor inventory.
#11/#13 retain concrete advisory/vendor qualification rather than merely link maps.
#8/#46/#183 have distinct image/backend scopes. Broad feature, standards, runtime,
retirement and release owners remain open where their explicit work is unfinished.
Do not change their acceptance to manufacture closures.
