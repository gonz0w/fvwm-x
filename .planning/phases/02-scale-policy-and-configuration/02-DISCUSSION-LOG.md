# Phase 2: Scale Policy and Configuration - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md; this log preserves the alternatives considered.

**Date:** 2026-08-30
**Phase:** 02-scale-policy-and-configuration
**Areas discussed:** Scale range and rounding, automatic DPI mapping, override config contract

---

## Scale Range and Rounding

### Accepted scale range

| Option | Description | Selected |
|--------|-------------|----------|
| Up to 4x | Covers very high-density displays while rejecting typo-like values. | Yes |
| Up to 8x | Allows extreme accessibility and specialty displays with larger resource risk. | |
| No policy cap | Accepts any positive quarter step that passes arithmetic safety checks. | |

**User's choice:** Up to 4x; reject configured scales below 1x.
**Notes:** The supported v0.1 domain is 1x through 4x inclusive.

### Rational conversion behavior

| Option | Description | Selected |
|--------|-------------|----------|
| Nearest, ties away from zero | Symmetric signed rounding without systematic shrinking. | Yes |
| Nearest, ties to even | Reduces statistical bias but is less intuitive per conversion. | |
| Toward zero | Simple but systematically shrinks dimensions and offsets. | |

**User's choice:** Nearest, with half ties away from zero.
**Notes:** Conversion overflow fails explicitly and never clamps; the user rejected
clamping and caller-specific overflow behavior.

---

## Automatic DPI Mapping

### Baseline and axis validation

| Option | Description | Selected |
|--------|-------------|----------|
| 96 DPI baseline | Conventional X/CSS physical-density baseline. | Yes |
| 110 DPI baseline | Delays automatic scaling on common laptop panels. | |
| Configurable baseline | Adds another policy setting beyond the required fallback. | |

**User's choice:** 96 DPI baseline.
**Notes:** Require both horizontal and vertical DPI, each within 50-500 DPI and
within 10 percent of each other. Combine accepted axes with the exact arithmetic
mean; never infer from one axis alone.

### Bucket and boundary behavior

| Option | Description | Selected |
|--------|-------------|----------|
| Nearest quarter, midpoint higher | Uses exact quarter steps and favors usable target size at ties. | Yes |
| Lower midpoint | Avoids increasing size until clearly over the boundary. | |
| Bias toward integer scales | Prefers whole factors over uniform quarter-step boundaries. | |

**User's choice:** Nearest quarter step, exact midpoint to the higher bucket.
**Notes:** Valid physical DPI outside the supported scale range uses the nearest
1x or 4x endpoint and records that boundary reason instead of rejecting the
automatic source.

---

## Override Config Contract

The user asked to accept all recommended choices rather than continue with
individual questions.

| Decision | Recommended contract | Selected |
|----------|----------------------|----------|
| Directive shape | Separate `MonitorScale <exact-name> <factor>` and `MonitorScaleFallback <factor>` directives. | Yes |
| Monitor identity | Exact, case-sensitive RandR Monitor name only; no numeric/current/primary aliases. | Yes |
| Factor syntax | Canonical decimal quarter steps from 1 through 4. | Yes |
| Duplicate handling | Last valid sequential declaration wins; invalid input preserves prior valid state. | Yes |
| Absent monitor | Retain the exact-name override and diagnose that it is currently unmatched. | Yes |
| Invalid active match | Reject ambiguity and continue through automatic DPI, global fallback, then safe 1x. | Yes |

**User's choice:** Use all recommended choices.
**Notes:** Exact diagnostic transport and internal representation remain agent
discretion, subject to visible fallback explanations and the fixed phase scope.

---

## The Agent's Discretion

- Internal policy file and API boundaries.
- Diagnostic wording, reason enum names, and storage layout.
- Exact existing Status/PrintInfo/log integration used to satisfy SCALE-04.
- Native test fixture organization.

## Deferred Ideas

None introduced during discussion. Arbitrary percentages and runtime reload were
already assigned to future requirements and Phase 6 respectively.
