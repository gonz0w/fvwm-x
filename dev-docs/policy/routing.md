# Codex routing and resources

These local clauses were extracted from [PROJECT.md](../PROJECT.md) without changing their
requirements. Source identity is the consumer revision containing this extraction.
Command paths and literal configuration paths remain relative to the repository root.
Selected applicability is recorded only in root `policy.toml`; linked contracts retain
their existing scope. The owner changes required policy through the existing process.

## Codex routing selection and policy change — 2026-10-24

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
[temper_product](../../.codex/agents/temper_product.toml).

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
