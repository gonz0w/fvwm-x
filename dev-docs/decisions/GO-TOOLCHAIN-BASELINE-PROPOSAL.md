# Optional Go compiler security baseline

Status: proposed 2026-10-14; user dependency decision required. Owner: LIB-03.
No compiler requirement or package has changed under this proposal.

## Concrete change

For Go-enabled builds, require a maintained patched stable compiler: Go 1.26.9 or
newer within the 1.26 series, or Go 1.27.2 and newer. Refuse Go 1.27.0/1.27.1
and older series during configuration with a clear explanation. Check the
actual compiler; do not trigger automatic toolchain downloading. Keep Go
optional and preserve the existing Go-disabled utility path.

Keep `go.mod`'s 1.21 language directive and existing vendored dependency pins
unless a separately reviewed change needs newer language/library APIs. Explain
the distinction between source-language compatibility and supported compiler
security in INSTALL.md and developer documentation. Do not change FVWM command,
binding, function, theme or configuration formats.

This raises the documented external compiler requirement. In particular, the
Linux ARM64 verification guest's Go 1.22.2 and five isolated Go 1.27.0 SDKs
would need batch-owned patched SDKs for future Go acceptance. Existing system
compilers/packages and prior evidence stay intact. The current audit/cleanup
can be reviewed independently.

## Why

The [affected audit](../verification/VERIFICATION-2026-10-14-GO-DEPENDENCIES.md)
finds no reachable affected third-party symbol on the six supported targets.
It removes unused Logrus without upgrading the six used modules. The older
Go 1.22.2 scan does report standard-library calls; platform/input triage reduces
several observations but does not justify supporting old unpatched compilers
indefinitely. A local patch for each old standard-library behavior would add
maintenance and would not restore upstream support.

Go's [release policy](https://go.dev/doc/devel/release) supports the two newest
major series. On 2026-10-14 those are 1.26 and 1.27; the latest security patches
are 1.26.9 and 1.27.2, released October 8. Recheck advisories and patch versions
when implementing; these dates are evidence, not an evergreen assertion.

## Reviewable implementation and acceptance plan

1. Add a small testable version policy to the existing configuration path,
   including 1.26.8/1.26.9, 1.27.0/1.27.1/1.27.2, newer versions, malformed
   output and Go-disabled controls. Keep the same optional feature behavior.
2. Acquire authenticated patched SDK inputs in isolated local/guest prefixes
   using the existing provider/bootstrap conventions. Record exact tool/source
   identity; do not overwrite system packages or other chats' SDK/build trees.
3. Re-run the pinned advisory analysis using the actual patched compilers.
   Retain findings and distinguish module/package/symbol and target conditions.
4. Qualify one combined offline six-target Go checkpoint, reusing owned caches
   per compiler/profile. Run affected utility contracts/install and live-X
   consumers required by the changed compiler; keep C/runtime identities and
   prior unrelated acceptance intact. No hosted CI or release is implied.
5. Update INSTALL.md, tracker, source inventory, security status and roadmap
   together. Refresh the supported patch baseline as part of dependency upkeep.

Recommendation: approve the patched optional build baseline. Keeping the old
floor with advice alone would keep default builds admitting unsupported SDKs.
The user chose secure defaults elsewhere; this separate external requirement
still needs an explicit decision.
