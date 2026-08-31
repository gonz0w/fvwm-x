---
status: testing
phase: 04-wm-owned-ui-metrics-and-resources
source: [04-VERIFICATION.md]
started: 2026-08-31T20:07:26Z
updated: 2026-08-31T20:07:26Z
---

## Current Test

number: 1
name: Picture cache held-out lookup
expected: |
  Same name bytes miss across quarters or generation; matching quarters, target_width, target_height, generation, name, stamp, and fpa_mask hit.
awaiting: user response

## Tests

### 1. Picture cache held-out lookup
expected: Same name bytes miss across quarters or generation; matching quarters, target_width, target_height, generation, name, stamp, and fpa_mask hit.
result: [pending]

### 2. Flocale cache held-out lookup
expected: Cache hit requires matching name bytes, scale_quarters, and generation; a quarters or generation mismatch misses.
result: [pending]

## Summary

total: 2
passed: 0
issues: 0
pending: 2
skipped: 0
blocked: 0

## Gaps
