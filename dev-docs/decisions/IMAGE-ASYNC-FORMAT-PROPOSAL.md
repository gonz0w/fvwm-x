# Ambiguous core image formats — approved

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Owner: IMAGE-01 / INPUT-01. Status: approved by the user on 2026-10-06. The approved
SVG completion contract remains in implementation. The optional loader is not
installed in core startup; the candidate described here is preparation only.

## Compatibility boundary

FVWM accepts supported image data even when its filename names another format.
For unknown suffixes the existing loader tries XPM, SVG, PNG, then bitmap; SVG
suffixes and rendering options prefer SVG. A compressed `.gz` or `.Z` input can
contain XPM or SVG. Its type cannot reliably be determined from its small
compressed header. Keeping a synchronous XPM expansion before the SVG attempt
would leave an up-to-five-second parent wait in the intended asynchronous path.

The candidate probes at most 16 bytes through the existing checked regular-file
open policy, with no allocation. Definite PNG and ordinary uncompressed XPM/XBM
headers keep immediate loading. Ambiguous inputs retain only their key and
opened identity while queued. Only the active request acquires full source and
output buffers. The active request preserves decoder order and uses the shared
owned process transport for compressed XPM expansion; nested service pumps
transport only. SVG receives the original snapshot if XPM expansion fails.
A failed XPM expansion does not impose its 128 MiB expanded limit on SVG.

This also changes completion timing for compressed XPM and some unusual or
misnamed raster/bitmap inputs: commands can return before final dimensions and
pixels. It does not change accepted data, syntax, decoder order, or finite
limits. The previous usable image remains until success; a first load uses
absent-image behavior. Explicit replacement, owner destruction and stale scale
results still cancel or discard interest.

## Decision

The user approved extending the completion contract to ambiguous core image
inputs, including compressed XPM. Leave utilities and modules synchronous, and
leave ordinary PNG and readily identified uncompressed XPM/XBM immediate.

The alternative considered was retaining synchronous non-SVG timing. That keeps
compressed XPM expansion before an unknown-suffix SVG attempt and limits the responsiveness
claim to requests whose SVG preference can be established without that wait.

AGENTS.md requires: “A specific compatibility break requires an explicit user
decision.” The earlier SVG-only decision does not explicitly cover compressed
XPM completion timing. The user explicitly approved that change on 2026-10-06.
Core activation still requires the resource-owner/native-X integration gates.

## Candidate checks

The candidate covers SVG-named XPM fallback, immediate ordinary XPM, compressed
XPM precedence with one owned decompressor, source identity changes, shutdown,
owned X resource refusal, and bounded prefix reads with open/read/close failures.
Preparation checks do not establish real window-manager responsiveness; the
remaining core owner and Linux/default BSD X checks remain required.

[Preparation report](../verification/VERIFICATION-2026-10-06-SVG-ASYNC-PREPARATION.md):
56 focused executions across seven compiler legs and seven scoped sanitizer
entries pass. Core activation, full suites and installed/default BSD X adoption
remain unverified.
