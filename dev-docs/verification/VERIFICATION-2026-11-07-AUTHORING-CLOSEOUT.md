# Reload and inspector lifecycle checkpoint

Historical U1 result. [U2 completes the remaining tail](VERIFICATION-2026-11-07-AUTHORING-TAIL.md)
without relabelling these retained failures. Use that report for current acceptance.

Base: merged PR368, `656a4cc7316faf873e4aa97c098f11d843cfc4ff`.
Owner: Cam. U1 batches RELOAD-01 #217 and INSPECT-01 #218; the tracker owns
acceptance. This report separates accepted implementation from the final
reload-coupled lifecycle checkpoint. Eight focused lifecycle cases pass.
The full campaign is stopped at its attempt limit after two retained fixture failures; neither issue is ready to close.

## Reused evidence

- [PR266 inspector](VERIFICATION-2026-10-26-NATIVE-INSPECTOR.md): six bounded
  sections, registry/shared completion, literal values, override explanations,
  layout, large state, filter/page/refresh, keyboard/focus and lost/restarted
  providers. Three configurations and nine inspected captures; affected Go race,
  native Form and scoped sanitizer checks. No exact dynamic runtime origin or
  per-widget attribution is inferred.
- [PR268 diagnostics](VERIFICATION-2026-10-26-CONFIGURATION-DIAGNOSTICS.md):
  source/span/include ownership, correlated module diagnostics and bounded native
  presentation. Eight affected profiles on six targets pass 64 policy, eight
  core-X and 18 actual-OS Go executions. This is source-diagnostic coverage,
  not a substitute for the inspector's own runtime contract.
- [PR368 reload](VERIFICATION-2026-11-07-THEME-UI-CLOSEOUT.md): strict bounded
  palette preparation/publication, actual Slate/Dune/enlarged Classic recoloring,
  sourced invalid-input refusal, surviving browser/editor/settings identity,
  unchanged wallpaper and an actual retained-draft callback. Four unmodified
  captures and candidate/base manuals are reused. Five affected Go package
  passes and two configured helper builds remain source/binary-bound.

The finite reload component is semantic colorsets 0–8 and 10–14. Wallpaper,
image assets, fonts, geometry, arbitrary configuration replay and external
Colorset overrides are outside that component. Reload publication is ordered
submission through the core command stream, not cross-command native rollback
or proof of every client's rendered completion.

## Final lifecycle checkpoint

One existing native Linux ARM64 profile, Slate only, forwards genuine worker
requests/results through a two-second fixture gate. No production code changes,
Go commands, builds or repeated OS/install matrix are included. Five package
passes, two helper builds and manuals are reused after 796 helper-source and
binary/tool/library identity checks. Other themes reuse accepted PR368 captures.

Both runs pass these eight distinct cases (not sixteen unique tests):

- One admitted preparation/success for duplicate pending no-op requests;
  repeated identical requests publish no Colorset commands.
- Missing and partial input retain the working component, emit source-aware
  errors, and recover on a fresh valid request.
- Actual worker death retains the component and allows a fresh request.
- A prepared old settings generation is refused; a fresh request can publish.
- An inspector collecting old colors retains its native window/children through
  reload, marks the result stale and refreshes; retired frame callbacks do not
  replace its successor. Unsubmitted filter text is not directly observed.
- Controller/provider loss while a real completed palette result is held and
  native contrast preview is active retires the worker, restores the native
  preview resources, makes the core slot available and emits no late colors.

The replacement baseline fails before normal preview recovery, external preview
conflict and closed-inspector late-result cases run. Those assertions are
implemented but unqualified. Existing generic preview/callback evidence is
retained; it does not turn these unexecuted reload-coupled cases into passes.

The unmodified stale-inspector capture from the first passing case is inspected
by the lead and separate reviewer. The full warning, old policy values and
controls are readable. [Gallery](../../doc/development-preview.md#stale-inspection-during-reload)
· [Capture manifest](../../doc/screenshots/theme-reload-lifecycle-manifest.json).

## Retained diagnostic and correction

The first native command runs 7.064 seconds. Eight cases pass, including actual
prepared-theme/controller loss with native preview recovery and the stale-color
inspector capture. Replacement startup then returns `provider_not_ready`, which
the fixture incorrectly treats as fatal. This is a fixture readiness error,
not evidence of a production reload defect. The 103-file export and archive
verify; no observer X or cleanup errors occur and the command group retires.
The owned container and machine are stopped.

The diagnosed correction permits that response only in the readiness query,
within its unchanged ten-second deadline. All other errors, owner identities,
exact roots/pixels and retained-state assertions remain binding. The first run
is retained as a failed run, not reclassified as a pass.
A second VM temporarily blocks the corrected job before any owned start or native
dispatch. After that VM ends, a separately reviewed fresh host output
path resumes the byte-identical guest bundle and same unexecuted correction.
No VM cap, other-owner resource or native attempt budget changes.

The corrected native command runs 7.156 seconds and passes the same eight cases.
Replacement readiness now succeeds; the next retained-state check fails exact
wallpaper-root equality after replacement editor setup. The fixture samples the
replacement baseline before editor setup, unlike PR368's settled baseline after
editor setup and a barrier. The failing expected/current replacement root pair
was not serialized, so the exact differing field and production cause remain
unavailable. Do not weaken equality or call this a demonstrated product defect.
All 107 export files/archive verify, the command group is absent, no observer X
or cleanup errors occur, and owned container/machine shutdown is confirmed.

Read-only source diagnosis confirms that an old valid root can coexist with
pending asynchronous replacement. The replacement controller rereads the edited
palette and submits a new Colorset20 source; ownership/corner equality alone
does not establish completion of that latest source. This is an insufficient
fixture baseline, while the actual differing root field remains unknown.
A source-only diagnostic follow-up now records both expected/current root
objects before exact equality and names the replacement baseline. It preserves
the assertion, scope and deadlines; this follow-up has not run natively.

The native campaign is stopped at its one-diagnostic/one-correction limit.
Preserve both failures and replan a bounded replacement-baseline diagnosis plus
remaining tail; this draft checkpoint does not authorize another native retry.

## Acceptance map

| Owner / criterion | Evidence and remaining gate |
| --- | --- |
| #217 finite stages and ownership | PR368 source/policy; U1 actual worker/provider retirement passes; normal recovery/conflict tail remains |
| #217 invalid/missing/failed preparation | PR368 invalid source; U1 missing/partial/worker death and fresh retry pass |
| #217 surviving UI identity | PR368 real editor/browser/draft/settings; U1 inspector window/children identity passes |
| #217 repeated/conflicting/stale/teardown | Repeated/stale/provider-loss pass; normal recovery/conflict and closed-result tail remain |
| #217 source/profile/screenshots | PR368 three configurations; U1 Linux partial results, full lifecycle not passed |
| #218 source/override/layout/unavailable inventory | Settings resolution and literal spans are covered; theme/binding rule ownership remains open after separate review |
| #218 shared completion without execution | PR266 shared metadata and generated catalog parity |
| #218 bounds/redaction/freshness | PR266 bounded/redacted/stale providers; U1 old-color collection truthfully marked stale |
| #218 large state/keyboard/provider loss | PR266; U1 old-frame callbacks pass, closed-result completion case unexecuted |
| #218 source/profile/screenshots | PR266/268/368 and U1 inspected stale capture; affected lifecycle tail open |

## Scope and costs

Full install/default-server/compiler/feature, native-CPU, physical, accessibility
and release acceptance retain their existing owners. This checkpoint changes no
OS API, native ABI, installation contract, build option or dependency. Broader
release acceptance is not inferred from feature closure.

The periodic performance comparison remains due. The current host has one other
owner's VM, so this functionality checkpoint cannot provide a quiet matched
comparison. No new performance infrastructure, reference rebuild, speed, RSS or
memory-parity claim is selected. Existing binaries and performance references
remain retained under the workflow cadence.

Requested Temper routes are implementer for the Python fixture and separate
reviewer for source/closure/execution inputs; workflow supplies a bounded evidence
map. Requested model settings are configuration, not independently attested
backend execution identity. Lead owns all native execution and integration.

## Evidence and continuation

[Machine-readable receipt](VERIFICATION-2026-11-07-AUTHORING-CLOSEOUT.json) names
plans, commands, archive hashes, passed cases and retained failures. Canonical
private artifacts are under `builddir/codex-2026-11-07/r1-authoring-closeout`:
first capture has 103 files, corrected `correction02` has 107. The host-only
`correction01` refusal has no native dispatch. The two runtime archives are
`419fb2f89cd6d63bc3fe065e1724c170a92566f72e5da4578844b905098d908a`
and `c1dba60bea1bdb28671e563c140bdc7cbcd30aeb423cd92f530d1d7405a0c5d9`.

No Closes marker is justified. #217 retains the replacement diagnostic and three
unexecuted lifecycle cases. #218 additionally retains static theme/binding
ownership and effective-rule explanations; unavailable dynamic origin does not
by itself satisfy that frozen static inventory criterion. Source-only authoring
of tests and a component PR do not close either owner. The next bounded work
must preserve passing cases and repair the diagnostic before native dispatch.
