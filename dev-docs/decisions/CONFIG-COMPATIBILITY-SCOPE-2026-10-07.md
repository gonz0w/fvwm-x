# Configuration compatibility scope

Accepted user clarification, 2026-10-07. The user states that the intended
compatibility requirement was configuration files only. This supersedes the
blanket theme, module-contract and legacy-behavior preservation language in
earlier project guidance. PROJECT-TRACKER.md retains execution authority.

## Required compatibility

Existing FVWM configuration files and their format are the compatibility
boundary. Keep supported command/directive names, arguments, bindings, functions
and theme configuration usable without requiring users to rewrite their files.
Parsing alone is not sufficient if a configured feature is removed. A deliberate
configuration break or required migration needs an explicit decision. Previously
approved FvwmPerl/Perl API retirement remains a recorded exception.

The user reiterated this stopping rule on 2026-10-07 after an unnecessary pause
for RaiseOverUnmanaged allocation failure. That change needs no configuration
rewrite, so its compatibility approval hold is withdrawn. This clarification is
current authority, not an additional compatibility gate.

## Boundaries

Internal APIs, implementation structure, native module protocols, external
binary compatibility, exact legacy pixels and incidental timing/error behavior
are not compatibility preservation requirements. Graceful allocation refusal
instead of process termination is not a configuration-format break. Update shipped producers/consumers
together and document changed public interfaces. Ask for a specific decision
when a proposed change deliberately breaks configuration compatibility, rather
than for every internal or protocol change.

This clarification does not authorize a broad rewrite, remove approved product
features or change Linux/BSD support, X11 correctness, memory/resource safety or
required platform qualification. Internal ordering/lifetime barriers remain
correctness obligations where necessary, independently of FVWM compatibility.

## Backlog application

COMPAT-01 now owns configuration compatibility. Tests of current module
transports remain correctness tests; they do not require preserving an old
external ABI. Reconcile remaining compatibility-only backlog work at its owning
checkpoint and record concrete retained/deferred scope. Do not delete tests,
invalidate accepted evidence or claim a shorter schedule without that review.
Historical decisions/reports retain their original scope and are not rewritten.
