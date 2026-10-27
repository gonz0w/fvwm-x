# FVWM-X project

FVWM-X modernizes FVWM3 into a responsive, dependable X11 desktop on Linux,
FreeBSD and OpenBSD. Keep FVWM's efficient single-threaded window-manager core,
process-isolated modules and compatibility with existing configuration files.

Current work, status and execution order belong to
[PROJECT-TRACKER.md](PROJECT-TRACKER.md). This document records scope and the
requirements migration; it is not another backlog.

## Product constraints

Read the canonical [product and compatibility contracts](policy/product.md#product-constraints).

## Monitor and HiDPI requirements migration

Read the canonical [product and compatibility contracts](policy/product.md#monitor-and-hidpi-requirements-migration).

## Documentation ownership

Read the canonical [documentation and method maintenance](policy/documentation.md#documentation-ownership).

## Agent Temper composition and project decisions

Adopt the pinned [Agent Temper method](../.agent-temper/METHOD.md) as the portable development
method. The [source record](../.agent-temper/SOURCE.md) identifies the immutable revision and retained
bytes. Only the [Codex profile](../.agent-temper/profiles/codex.md) is selected. Existing local
verification and GitHub practices remain local extensions; the bundled GitHub
and strict-test-first profiles are inactive.
This project document and its linked product decisions own product scope. This policy owns development decisions and
authority; it does not replace those product contracts.

### Work authority and finite delivery

Read the canonical [work and delivery contracts](policy/work.md#work-authority-and-finite-delivery).

### Local contracts and simplicity

Read the canonical [product and compatibility contracts](policy/product.md#local-contracts-and-simplicity).

### Verification and review

Read the canonical [verification and execution contracts](policy/verification.md#verification-and-review).

### Decisions and action authority

Read the canonical [review and action authority](policy/authority.md#decisions-and-action-authority).

### Documentation for readers

Read the canonical [documentation and method maintenance](policy/documentation.md#documentation-for-readers).

### Maintenance of the method

Read the canonical [documentation and method maintenance](policy/documentation.md#maintenance-of-the-method).

### Codex routing selection and policy change — 2026-10-24

Read the canonical [codex routing and resources](policy/routing.md#codex-routing-selection-and-policy-change--2026-10-24).

## Terra product routing update — 2026-10-27

Read the canonical [codex routing and resources](policy/routing.md#terra-product-routing-update--2026-10-27).

## Explicit policy selection

The owner-selected modular adoption in [issue #282](https://github.com/gonz0w/fvwm-x/issues/282)
uses root [policy.toml](../policy.toml) as the sole selected-module record. Read every
selected module and its declared prerequisites before work; linked local contracts
retain their existing applicability. PROJECT remains the entry point and retains
its prior heading anchors as pointers to the canonical extracted clauses.

Compatible stronger rules apply together. The owner controls required-policy,
selection, exception and authority changes through the existing work/review process.
The manifest describes composition; it does not authorize actions or supersede prose.
Historical finite approvals remain expired for this work; selection does not renew
WIP lanes, host permissions, check/hook waivers or operational authority.

The [manually invoked selection checker](../tools/temper/SOURCE.md) reads inert declared data.
It is outside the retained method bundle and is not wired into CI, hooks or gates.
Mechanical success leaves policy meaning, source trust, approval, check applicability
and execution/integration authority unverified. Use only an already authorized executor;
missing required checks remain unavailable and block their dependent acceptance.
