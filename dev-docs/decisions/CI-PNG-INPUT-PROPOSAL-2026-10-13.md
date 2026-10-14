# CI-01 patched PNG input — 2026-10-13

Status: approved by the user on 2026-10-13; implementation and fresh-provider
qualification are locally accepted. See the [verification report](../verification/VERIFICATION-2026-10-13-CI-PNG-BOOTSTRAP.md).
Workflow adoption and actual native release CI remain separate. The approved minimum version and existing FVWM
configuration contract remain unchanged.

The new pinned FreeBSD 15.1 provider images boot and run authenticated default
Xorg probes on both architectures. Their freshly installed `png` package reports
1.6.58. The user already approved requiring 1.6.59 or a verified vendor security
backport. The ordinary candidate preparation has no verified backport assertion,
so these packages cannot qualify a PNG-enabled core build.

## Recommended option

Add a fixed, verified CI source bootstrap for the existing libpng dependency.
Use the previously reviewed upstream 1.6.59 release, not a moving branch:

- URL: `https://download.sourceforge.net/libpng/libpng-1.6.59.tar.xz`
- Official SHA256: `d80dd2a38a37f803cb9b6ac7b14bd6e74ddc3b654780a8380bdf93523fdb4389`
- Existing receipt: `builddir/codex-2026-10-05/png-security-policy/source-provenance.json`

Implement finite verified acquisition and an owned build/install within each
fresh CI job. Retain the release license, configure/build/upstream-test logs,
source and compiler hashes, actual installed library identities and loader
observations. Use base system/Python tools and existing build dependencies.
Do not introduce a new WM library, GUI toolkit or configuration migration.
Do not assert a backport for an unverified package.

Inspect the platform's library names and actual core/SVG-worker consumers.
A private prefix alone is not evidence that every relevant loaded PNG consumer
uses the patched library. Qualify consistent loader/install behavior before
adopting this fallback. Earlier private-prefix local qualification is retained;
it does not establish a passing new provider/candidate bootstrap.

Then run the new provider's fresh standard/Go/staged/default-X checkpoints once
as a coherent dependency/provider batch. Keep actual remote CI, release sanitizer
execution, physical hardware and publication separately gated.

## Alternative

Keep the candidate inactive until the BSD feeds provide 1.6.59 or newer, or a
vendor package/backport with the required independently reviewed fixes. Qualify
those actual packages and all four provider profiles when available. No version
check or operating-system name substitutes for that evidence.

## Why this is a decision

The QEMU wrapper replacement is already approved and implemented. Adopting a
source bootstrap adds an external CI acquisition/build-input path in place of
relying solely on package feeds. The user requested a decision for changes to
external dependencies; this proposal makes that delivery choice explicit. It
is not a FVWM configuration compatibility decision or a request to lower the
approved dependency baseline.
