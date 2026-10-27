# FVWM-X project

FVWM-X modernizes FVWM3 into a responsive, dependable X11 desktop on Linux,
FreeBSD and OpenBSD. Keep FVWM's efficient single-threaded window-manager core,
process-isolated modules and compatibility with existing configuration files.

Current work, status and execution order belong to
[PROJECT-TRACKER.md](PROJECT-TRACKER.md). This document records scope and the
requirements migration; it is not another backlog.

## Product constraints

- User direction, 2026-10-26: target a highly configurable, fast, low-memory
  desktop lite. All currently planned features target R1; finite remaining
  maintenance moves to R2/R3. Existing evaluations finish in R1 and implement
  accepted choices. Follow the [three-release plan](decisions/RELEASE-PLAN-2026-10-26.md)
  and tracker execution order instead of older cleanup-first scheduling.
  Major/Minor describes work breadth, not urgency. Preserve accepted components,
  platform/configuration scope and the risk-based testing cadence. Future feature
  intake goes to later releases unless R1 scope is explicitly changed.
- X11/XRandR 1.5+; no native Wayland backend or built-in compositor.
- OpenBSD follows new releases on AMD64 and ARM64, with 8.0 the current
  qualified baseline and 7.9 out of scope. Refresh fixed inputs and qualify
  both architectures when advancing the baseline. Linux and FreeBSD remain
  in scope; development snapshots do not substitute for release qualification.
  See the
  [baseline decision](decisions/OPENBSD-8.0-BASELINE-2026-10-20.md).
- Authoritative window/X state stays in the core's single thread. Bound queues,
  event-loop work and interaction servicing; preserve dispatch/command order.
- Scale FVWM-owned UI and geometry, not arbitrary application pixels. EWMH,
  ICCCM, configure and module geometry remain actual X11 pixels.
- Compatibility is limited to existing FVWM 3.x configuration files and their
  format. Keep supported names, arguments, bindings, functions and theme
  directives usable without a configuration rewrite. Ask only for a deliberate
  configuration incompatibility or required migration. Internal APIs/ABIs,
  module protocols, exact legacy rendering, incidental timing and failure
  behavior have no separate compatibility hold; update shipped consumers,
  documentation and verification together. Other product constraints remain.
  See the [clarified compatibility scope](decisions/CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
- Approved dependency simplification, 2026-10-23: mandatory Go for building,
  complete first-party Perl/Python retirement, FVWM 3.x-only compatibility,
  obsolete converter/backend removal and target-owned linkage. Keep Meson until
  the bounded Muon/CMake evaluation produces a separate adoption decision.
  Accepted runtime/linkage/backend cleanup remains accepted; remaining Perl
  runtime retirement targets R2 and full first-party developer-tool migration R3.
  Backend/library decisions and the isolated Lua feature target R1. See the
  [decision and issue ownership](decisions/DEPENDENCY-SIMPLIFICATION-2026-10-23.md).
- Approved runtime language direction: retain C core/native primitives, replace
  first-party Python runtime helpers and portable Perl utilities with Go, and
  add optional isolated Lua policies over shared primitives. The user
  explicitly approves future retirement of FvwmPerl/perllib and Perl-specific
  APIs; unrelated FVWM/FvwmScript contracts remain. Accepted Go runtime/controller
  ports remain accepted; isolated Lua targets R1,
  Perl framework retirement R2 and full first-party build/test tooling R3.
  See the [component audit and scope](decisions/RUNTIME-GO-LUA-2026-10-06.md).
- The default desktop uses existing FVWM components and base-system capabilities;
  unavailable services are reported honestly. No additional desktop framework
  or mandatory GUI toolkit is introduced by documentation or cleanup work.
- Shipped geometric artwork uses self-contained SVG icons, wallpapers, palette
  previews and branding. The complete visual profile uses the existing isolated
  SVG renderer; minimal builds retain labeled controls and plain backgrounds.
  PNG remains appropriate for screenshots and existing raster-image support.
  See the [artwork decision](decisions/THEME-SVG-ARTWORK-2026-10-06.md).
- Approved session direction: a capable portable session manager in the default
  install, with comprehensive optional D-Bus integration extending the same
  settings/actions/UI. Missing optional services must not prevent login. Runtime
  work remains queued; see the [session scope](decisions/SESSION-INTEGRATION-2026-10-06.md).
- Approved file-manager direction: reusable native collection, directory and
  action primitives first, then reliable file-operation jobs, optional desktop
  pinboard and application interoperability. A separate config/theme build
  thread can consume each qualified primitive checkpoint while core work
  continues. Advanced features remain individually deferred candidates; no
  mandatory GUI toolkit or new Open/Save portal backend is implied. See the
  [file-manager scope](decisions/FILE-MANAGER-PRIMITIVES-2026-10-06.md).
- Approved clipboard direction: reusable typed content, selection transfer,
  retention and protected-storage primitives first, then a text/image consumer
  handoff for a separate configuration/theme thread while runtime work continues.
  Reuse the file-manager collection/action contracts. Protected memory precedes
  optional encrypted persistence; password retention and defaults remain open.
  X11 display clients remain outside the confidentiality guarantee. See the
  [clipboard scope](decisions/CLIPBOARD-PRIMITIVES-2026-10-06.md).
- Approved desktop exploration direction: additional consumers and hardware,
  application and richer-layout capabilities wait until the existing program is
  complete. Shared capability/state, typed-action, asynchronous-operation,
  collection-filtering and owned-setting preview/revert contracts are scoped
  into current CFG/IPC/SESSION/FILE work, using existing consumers. Future
  features add no release prerequisite or backend/dependency adoption. See the
  [shared scope and inventory](decisions/DESKTOP-EXPLORATION-2026-10-06.md).
- Requested late localization polish covers all eleven current catalog languages
  across first-party UI, manuals and shipped configuration comments after the
  existing program. Preserve executable configuration and user files, qualify
  language rendering/input, and keep translations usable offline. Tooling and
  service choices remain proposed. See the
  [localization scope](decisions/LOCALIZATION-POLISH-2026-10-08.md).
- Follow the six-target [support matrix](SUPPORT.md). Pure policy needs native
  tests; X-visible changes need real-X integration. Native Linux sanitizer execution
  and physical hardware/compositor qualification remain separate evidence gates,
  performed locally. GitHub hosts code/issues/PR review with Actions disabled.
- Adopt and qualify the applicable [standards contracts](STANDARDS.md) for the
  first release, with requirement-level evidence and honest profile-specific
  claims. Native defaults follow [WCAG2ICT-adapted theme guidance](THEME-STANDARDS.md);
  actual web exports target WCAG 2.2 AA. This does not mandate a new toolkit,
  browser engine, GTK/Qt theme creation or every optional desktop service.

## Monitor and HiDPI requirements migration

The original milestone files now live in
[history/planning](history/planning/README.md). Preserve their requirements,
decisions and verification history, but do not use their old checkboxes as
current passing evidence. Some checkboxes predate later repairs, and the old
requirements contain a compatibility policy superseded by the user.

Historical identifiers are written here with a `v0.1/` prefix. For example,
`v0.1/GEO-01` means monitor ownership, while the active tracker uses `GEO-01`
for the completed configure/coordinate fix. Historical `COMPAT-01/02` also
collide with current compatibility/Perl entries. Never import those IDs directly.

| Historical requirements | Current owner and qualification |
| --- | --- |
| v0.1/TOP-01, TOP-02, TOP-03, TOP-06 | Committed topology, ownership, burst recovery and live-X harness. Preserve existing monitor policy/tests; ongoing qualification belongs to MONITOR-01, COMPAT-01 and VERIFY-01. |
| v0.1/TOP-04, TOP-05, SCALE-06 | Monitor removal/recovery, inspection/simulation and selective scale reload. MONITOR-01 first reconciles current implementation and evidence before identifying missing behavior. |
| v0.1/SCALE-01 through SCALE-05 | Rational overrides, automatic DPI, precedence, diagnostics and supported quarter steps. Existing scale/config policy remains; MONITOR-01 verifies reload/transition dependencies. |
| v0.1/UI-01 through UI-06 | WM chrome/fonts/images/cursors/interaction scaling and resource keys. COMPAT-01 and IMAGE-01/DRAW-01/CACHE-01 retain the relevant contracts; MODULE-SCALE-01 audits bundled UI consumers. |
| v0.1/GEO-01 through GEO-07 | Ownership, logical size, ICCCM constraints, drag anchor, work areas, restore/fullscreen and physical-pixel protocol. Existing geometry/interaction tests are retained; MONITOR-01 reconciles hotplug/reload, with INPUT-01 for lifetime/interaction changes. |
| v0.1/MOD-01 through MOD-03 | Additive monitor/scale metadata and bundled module consumers. MODULE-SCALE-01 audits coverage before extending contracts, coordinated with IPC-02 and COMPAT-01. |
| v0.1/TEST-01 through TEST-05 | Arithmetic/geometry policies, nested-X topology and client/compositor/build profiles. VERIFY-01/COMPAT-01 and SUPPORT.md own qualification; no archived claim closes remaining physical-GPU/toolkit/native-execution gaps. |

Future ideas from the archived milestone remain candidates: per-application
pixel preservation, scale percentages outside the supported buckets,
reconnected-monitor placement and richer recovery history. They are not
approved compatibility changes or implicit additions to the current build.
Evaluate them against concrete needs under MONITOR-01/CFG/IPC before creating
new runtime work.

## Documentation ownership

Developer guides explain how to change and verify the product. User-facing
commands, options, services and examples belong in `doc/`, generating installed
man pages and optional HTML from one source. The documentation
[policy](DOCUMENTATION.md) and [workflow](WORKFLOW.md) define upkeep.

## Agent Temper composition and project decisions

Adopt the pinned [Agent Temper method](../.agent-temper/METHOD.md) as the portable development
method. The [source record](../.agent-temper/SOURCE.md) identifies the immutable revision and retained
bytes. Only the [Codex profile](../.agent-temper/profiles/codex.md) is selected. Existing local
verification and GitHub practices remain local extensions; the bundled GitHub
and strict-test-first profiles are inactive.
This project document and its linked product decisions own product scope. This policy owns development decisions and
authority; it does not replace those product contracts.

### Work authority and finite delivery

The [local tracker](PROJECT-TRACKER.md) is the sole authority for live scope, priority, status, dependencies and continuation. GitHub issues and Project fields are manually reconciled mirrors; they do not form a competing queue.

Select accepted, bounded work before implementation. Name its owner, outcome,
non-goals, acceptance, dependencies, verification and effects. Keep the approach,
discoveries and continuation in that work record. A broad milestone or unresolved
proposal is not permission to implement. Preserve pending broader acceptance when
a component merges. Follow the [existing workflow](WORKFLOW.md) for selection,
review, status reconciliation and completion. Method adoption does not advance
product qualification or erase failed/historical results.

### Local contracts and simplicity

C core/native primitives and the approved Go/Lua direction; X11 on Linux, FreeBSD and OpenBSD, existing FVWM configuration compatibility and single-threaded X authority. The current maintenance, dependency-retirement and risk-based test decisions in this project document remain authoritative.

Keep current languages, tools, dependencies and support decisions at their
canonical local paths. Use the smallest implementation that meets current
acceptance, safety, recovery and accessibility requirements. A new language,
service, dependency or abstraction needs a present benefit and maintenance-cost
justification; significant direction changes need an owner decision. Preserve
existing immutable Suite/tool/source pins and approved exceptions. The consumer
source revision binds these local extensions; external sources retain their own
recorded revisions, hashes and content. Temper imposes no OS, toolchain or hosting.

### Verification and review

Documentation-only changes use `make check`, working/staged whitespace checks and affected source/link review. Preserve the current larger-batch and risk-based cadence; no WM build, VM or broad matrix solely for this adoption.

Follow existing behavior-first/regression and PRD/EDD/Test Design obligations
where they apply. Prose changes use source, link, policy and reader checks; do not
invent red/green runtime history. Preserve final required checks and conservative
fallbacks. Record passed, failed, skipped, unavailable and not-applicable results
separately. A stored recipe or component merge is not native, browser, hardware,
performance, operational or release acceptance.

On 2026-10-24 the owner approved lasting bounded read-only agent reviews of
accepted work. The lead may assign a finite source/policy/evidence inspection to
a separate agent and record the reviewer, exact candidate, scope, findings and
resolution. This replaces any previous blanket no-agent rule for that review
scope only. The October 24 routing decision below additionally permits bounded
implementation delegation. Candidate/native execution, other-resource control
and merging retain their existing authority gates. Self-review remains self-review;
an agent review is not an independent human approval or a GitHub approving review.

### Decisions and action authority

Cam (@gonz0w) remains the owner. Agents may prepare accepted changes, choose
routine implementation/documentation details within scope, run authorized checks
and prepare signed, actual-author-DCO review candidates. Existing commit and PR rules remain. Use only the existing authorized signing identity;
do not create keys, trust files or alter security configuration to force a pass.

Leave PRs open for maintainer review. Current policy requires explicit maintainer
merge authorization; method adoption and review permission do not supply it.
Required checks, review findings and final source/signature/DCO verification must
be resolved before an authorized merge. Release, publication, deployment,
credentials, operational/native effects and changes outside the owning work item
retain their separate authority. The adopted method grants none of these.

If a required decision is missing or applicable rules conflict, record the
question in the owning work item, ask the owner and stop only dependent work.
Continue independent authorized work. Save the old decision, replacement, reason,
scope and authority here or in its canonical local decision record before
proceeding. Apply recorded answers in later sessions without asking again.
Compatible rules apply together; file order, a newer upstream timestamp or an
agent preference does not override them. No Temper core deviation is declared.

### Documentation for readers

Use the [documentation reference](../.agent-temper/docs/reference/documentation.md)
for reader-oriented structure and original STE-informed plain-English procedures.
Do not claim formal ASD-STE100 compliance or reproduce its dictionary. Put the
result, prerequisites, working directory and meaningful recovery in task guides;
use consistent terms and synthetic examples. Separate reference from explanation
when readers benefit, without creating empty sections or duplicate documents.

README explains purpose, maturity and navigation; agent instructions provide entry
routes; contribution guidance explains delivery; this policy records decisions.
Product/user documentation owns supported behavior and setup/recovery. Decisions
explain tradeoffs; evidence retains actual source-bound observations. Keep live
work in its authority above. Preserve the existing installed/manual and browser
accessibility requirements, and distinguish proposed setup from executed support.

### Maintenance of the method

Use the existing work/review process for pin or extension updates, exceptions and
removal. Review semantic changes, compatibility, source retention, links/licenses
and a representative work item from pickup through completion. Owner decisions
are needed for authority or required-policy changes. Adoption findings belong in
[the owning tracker checkpoint](PROJECT-TRACKER.md#agent-temper-adoption--maint-02temper-01), mirrored in [issue #192](https://github.com/gonz0w/fvwm-x/issues/192); publish a sanitized upstream proposal only with
appropriate authority. A project-specific requirement is not a universal rule.

### Codex routing selection and policy change — 2026-10-24

The owner explicitly selected model-routed subagent implementation and directed
all existing Temper adopters to be ready for it in
[issue #196](https://github.com/gonz0w/fvwm-x/issues/196). This replaces the
previous read-only-review-only delegation allowance with bounded implementation
delegation within already owner-accepted task scope. The lead must supply explicit
file ownership, acceptance, policy, evidence, stop conditions and resource limits.
The reason is that owner-selected workflow; separate review and existing native,
operational and per-PR maintainer merge approval remain binding. This does not
select a new product/platform/toolchain or expand other-resource authority.

| Role | Assignment | Model | Effort |
| --- | --- | --- | --- |
| `temper_workflow` | Bounded read-only inspection and evidence summaries | `gpt-6-luna` | `medium` |
| `temper_product` | Bounded documentation, visual checks, authorized browser tasks and small specified UI fixes | `gpt-6.1-terra` | `medium` |
| `temper_implementer` | Accepted implementation, tests and ordinary debugging | `gpt-6.1-sol` | `high` |
| `temper_analyst` | Difficult analysis or separate complex review | `gpt-6-astra` | `high` |
| `temper_reviewer` | Separate final review of work it did not author | `gpt-6.1-sol` | `high` |

The lead owns tracker writes, integration and returned-evidence verification.
Use at most three concurrent delegated agents, excluding the lead, within existing
resource and WIP limits. No nested helpers without an explicit lead assignment.
Keep tiny or tightly coupled work with the lead when appropriate. No role changes
permissions or authorizes native/operational execution by itself.

If a role or model is unavailable, report it. The lead may do the bounded work
with its current model only when adequate and permitted under existing policy
and limits; otherwise resolve the missing choice with the owner. Never silently
substitute a model or claim that requested settings actually ran. The owner
approves model/effort/role/limit changes; routine routing within this table is
delegated to the lead. Preserve independent final review.

Local `.codex/agents/` files are adapted project configuration outside the pinned
bundle. No global/default model, permission or primary-chat setting changes.
Where native role discovery is unavailable but explicit spawn settings are
supported, use this same table with a complete fresh-context brief and record
observed settings, or follow the fallback. Existing chats do not switch models
when files change. Record client/version and observed role/model/effort metadata
in the owning issue; parsing proves syntax only. Native discovery/routing remain
unverified until actually observed in a fresh supporting session. Earlier active
work retains accepted scope and evidence; future delegation follows this policy.

## Terra product routing update — 2026-10-27

The owner authorized this workflow/model-routing update and integration after
review in [the owning issue](https://github.com/gonz0w/fvwm-x/issues/272). Select `temper_product` with
`gpt-6.1-terra` / `medium` from the recorded upstream pin. This adds product-facing
work to the selected routes; the existing workflow, implementer, analyst and
reviewer settings remain unchanged. The local role is
[temper_product](../.codex/agents/temper_product.toml).

Use the product role only for accepted documentation, visual inspection, explicitly
authorized bounded browser tasks and small UI fixes with specified behavior and
observable acceptance. Substantial implementation stays with the implementer;
ambiguous design goes to the analyst. Existing risk, platform, execution and
separate-review requirements override a routine product assignment. The author
cannot supply its own required separate review.

For this role only, the lead may explicitly select `temper_implementer` with
`gpt-6.1-sol` / `high` as the one approved fallback when Terra is unavailable or
cannot perform the task with available tools. Record that selection and confirm
capability, tool availability, existing permissions and task/resource limits
before dispatch. Missing tools are not installed by choosing another model.
Retain stricter dispatch/risk rules and stop if the fallback is unsuitable. Other
routes keep their existing fallback policy; no automatic substitution, recursive
delegation, extra retry or higher limit follows.

Terra requires a supporting client/account; Codex CLI requires 0.159.0 or newer.
Start a fresh consumer session and inspect actual role/model/effort metadata
before claiming observed routing. Configuration and source review do not prove
runtime discovery, model execution or measured quality/cost benefit. These remain
unverified until observed. This selection changes no primary-chat model, CI,
hooks, trust, permissions, product/platform or consequential-action authority.
