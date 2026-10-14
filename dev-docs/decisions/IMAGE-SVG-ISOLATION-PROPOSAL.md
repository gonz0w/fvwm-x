# SVG decoder isolation and resource limits — approved

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Owner: IMAGE-01 / PROC-01 / LIB-04. The user approved five seconds and
1024 MiB with finite overrides on 2026-10-05. The bounded security implementation
is accepted on 2026-10-06; parent waiting remains synchronous and responsiveness
integration is open. See the [qualified checkpoint](../verification/VERIFICATION-2026-10-06-SVG-ISOLATION.md).
The approved primary-file and compressed-XPM
limits are implemented. This is a separate compatibility decision about SVG
loading and rendering, including secondary images and SVGZ expansion.

## Observed remaining wait

The final staged Linux ARM64 core accepts an ordinary, small regular SVG whose
relative image refers to a task-owned empty FIFO. The initial command barrier
passes; the following barrier times out during a controlled two-second
observation. The core remains alive in the kernel's `wait_for_partner` state.
The harness cleans up its own processes afterward. The approved primary-file
check cannot check file opens performed inside librsvg for secondary resources.

Evidence: `builddir/codex-2026-10-05/image-input-integration/`, specifically
`svg-secondary-control.log` and the archived SVG review results. Ordinary SVG,
SVGZ, relative PNG images and existing `:options:path` syntax pass this batch's
valid-image checks. Removing those features is not the recommendation.

## Approved compatibility policy

Use a **five-second total SVG operation deadline and a 1024 MiB worker memory
limit**, with finite build-time overrides of 1–60 seconds and 128–4096 MiB.
The deadline includes startup, parsing, secondary-resource loading, rendering,
result transport and direct-child completion. Discard partial or late results;
return a normal image-load failure and allow later commands to recover.

This can reject otherwise valid slow or memory-intensive legacy SVG themes.
1024 MiB is proposed engineering headroom, not a measured maximum for existing
themes. Qualification must measure representative themes and maximum accepted
output before accepting the implementation. The approved five-second XPM and
GetOutput limits do not authorize this separate SVG policy.

The memory setting has explicit platform semantics: use `RLIMIT_AS` for virtual
address space on Linux/FreeBSD and `RLIMIT_DATA` for data allocation, including
anonymous mappings, on OpenBSD. These are not identical RSS limits. Preserve
any stricter inherited limit, check limit-setting failures, and reject startup
if the required containment cannot be established. Do not raise inherited hard
limits or claim that `RLIMIT_RSS` provides portable enforcement.

## Implementation boundary after approval

- Isolate SVG decoding in an owned process using the existing libc, process,
  transport, librsvg and Cairo dependencies. No new mandatory package or GUI
  toolkit. Decide the narrow worker entry point during implementation review;
  do not fork into third-party state without checking its safety.
- Hand off the already checked primary bytes and original base URI. Preserve
  ordinary SVG/SVGZ, relative regular-file resources, rendering options,
  scaling, fallback order, initial source identity and cache generations.
- Keep all X server operations and authoritative window state in the core.
  Validate returned dimensions, pixel counts, lengths and arithmetic before
  allocating or applying results: existing color-image limits remain 32768 per
  dimension and 67,108,864 pixels, with at most 256 MiB of ARGB output.
- Reuse checked descriptor handling and literal arguments; retain child
  identity until termination/reaping. Bound pipes, ownership and outstanding
  jobs. Preserve unrelated child notifications and shutdown cancellation.
- Keep command and image-result ordering explicit. Process isolation alone is
  not event-loop responsiveness: a synchronous five-second parent wait does
  not satisfy the responsiveness goal. Review the smallest integration that
  services required events without unsafe recursive commands or stale results;
  do not infer a broad asynchronous rewrite from this decision.

The deadline contains existing library work. It does not guarantee immediate
termination of an OS task in uninterruptible storage I/O, make every regular
file read nonblocking, or establish a filesystem sandbox. SVG secondary-path
permissions remain those of the user. Wider image isolation and permissions
have their own review scope.

## Required acceptance

Test native policy boundaries and worker startup/limit/transport failures,
partial output, timeout, cancellation, child notifications, descriptor cleanup
and recovery. Exercise actual SVG/SVGZ, relative resources, options, color/alpha,
scaling/cache ownership, secondary FIFO waits and memory refusal. Run all six
platform targets, installed BSD default X servers and scoped sanitizer checks;
native Linux release CI remains a distinct requirement. Record differences
between OS resource-limit behavior and measured responsiveness.

## Decision authority and sources

[AGENTS.md](../../AGENTS.md) says: “A specific compatibility break requires an
explicit user decision.” These time/memory limits can reject valid legacy SVG
assets. The user has now approved this policy;
the current approved acquisition batch is completed independently.

Library resource/base-URI behavior: [librsvg Handle documentation](https://gnome.pages.gitlab.gnome.org/librsvg/Rsvg-2.0/class.Handle.html).
Resource-limit semantics: [Linux getrlimit](https://man7.org/linux/man-pages/man2/getrlimit.2.html),
[FreeBSD getrlimit](https://man.freebsd.org/getrlimit%282%29),
[OpenBSD setrlimit](https://man.openbsd.org/setrlimit.2).

## Qualified implementation

The private worker, checked pixel protocol and common process transport are
implemented and qualified on six kernel/platform profiles. Limit readback fails
closed, stricter inherited limits remain, and the worker has a lifetime alarm.
The manual example and maximum 256 MiB output measurements fit the defaults;
this is not a third-party theme survey. Full-system Linux AMD64 QEMU replaces
user-emulated qualification because the latter does not enforce RLIMIT_AS.
The [report](../verification/VERIFICATION-2026-10-06-SVG-ISOLATION.md) records exact
gates, failed attempts, local sanitizer scope and synchronous-wait limitations.
