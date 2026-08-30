# Phase 1: Topology Snapshot and Live-X Harness - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md; this log preserves the alternatives considered.

**Date:** 2026-08-29
**Phase:** 1-Topology Snapshot and Live-X Harness
**Areas discussed:** Snapshot identity and inspection, atomic commit and epochs,
burst settling behavior, live-X harness contract

---

## Snapshot Identity and Inspection

| Option | Description | Selected |
|--------|-------------|----------|
| Approve recommendation | RandR Monitor name as runtime identity, complete sorted membership, raw dimensions, and versioned JSON plus derived text through an existing path | Yes |
| Adjust decision | Revisit identity or inspection behavior before context creation | |

**User's choice:** Approve recommendation.
**Notes:** The user delegated choices using the priority order modernity,
performance, then minimum complexity.

---

## Atomic Commit and Epochs

| Option | Description | Selected |
|--------|-------------|----------|
| Approve recommendation | Detached validated candidate, one atomic swap, last-good runtime retention, and semantic epoch advancement | Yes |
| Adjust decision | Revisit validation, failure, or epoch behavior before context creation | |

**User's choice:** Approve recommendation.
**Notes:** Enumeration and output ordering noise must not advance the epoch.

---

## Burst Settling Behavior

| Option | Description | Selected |
|--------|-------------|----------|
| Approve recommendation | One deferred refresh after queue settling with a monotonic maximum-latency cap and no blocking | Yes |
| Adjust decision | Revisit coalescing behavior before context creation | |

**User's choice:** Approve recommendation.
**Notes:** Exact timing constants are delegated to planning and must be justified
by live-X tests.

---

## Live-X Harness Contract

| Option | Description | Selected |
|--------|-------------|----------|
| Approve recommendation | Standard-library Python orchestration, Xvfb smoke coverage, one native Xorg-dummy lane, polling/timeouts, and retained artifacts | Yes |
| Adjust decision | Revisit harness language, CI split, or evidence before context creation | |

**User's choice:** Approve recommendation.
**Notes:** Live-X work should run once per appropriate CI tier rather than in
every compiler/distribution matrix leg.

---

## The Agent's Discretion

- The user delegated all phase-specific choices using the priority order:
  modernity, performance, then minimum complexity.
- Exact inspection command naming, versioned JSON field naming, coalescing
  constants, and harness fixture organization remain implementation discretion
  within the approved contracts.

## Deferred Ideas

None.
