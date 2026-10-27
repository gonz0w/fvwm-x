# Documentation and method maintenance

These local clauses were extracted from [PROJECT.md](../PROJECT.md) without changing their
requirements. Source identity is the consumer revision containing this extraction.
Command paths and literal configuration paths remain relative to the repository root.
Selected applicability is recorded only in root `policy.toml`; linked contracts retain
their existing scope. The owner changes required policy through the existing process.

## Documentation ownership

Developer guides explain how to change and verify the product. User-facing
commands, options, services and examples belong in `doc/`, generating installed
man pages and optional HTML from one source. The documentation
[policy](../DOCUMENTATION.md) and [workflow](../WORKFLOW.md) define upkeep.

## Documentation for readers

Use the [documentation reference](../../.agent-temper/docs/reference/documentation.md)
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

## Maintenance of the method

Use the existing work/review process for pin or extension updates, exceptions and
removal. Review semantic changes, compatibility, source retention, links/licenses
and a representative work item from pickup through completion. Owner decisions
are needed for authority or required-policy changes. Adoption findings belong in
[the owning tracker checkpoint](../PROJECT-TRACKER.md#agent-temper-adoption--maint-02temper-01), mirrored in [issue #192](https://github.com/gonz0w/fvwm-x/issues/192); publish a sanitized upstream proposal only with
appropriate authority. A project-specific requirement is not a universal rule.
