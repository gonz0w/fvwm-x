# PNG security baseline — approved 2026-10-05

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

The runtime recovery fix is [verified](../verification/VERIFICATION-2026-10-05-PNG-RECOVERY.md).
The package audit found that several verification targets have PNG packages
below the upstream 1.6.59 security fix, with no matching vendor backport proof.
Keeping an API version floor of 1.2 accepts those packages at configure time.

## Approved default

If PNG is enabled or automatically found, require libpng 1.6.59 or newer.
An explicit `-Dpng-security-backport=true` allows an older package only
when its vendor's fixes have been reviewed and recorded. The flag is the
builder's assertion, not an automated vulnerability verdict. Packaging and
verification profiles must record the exact package/advisory evidence. A package
that merely has an older upstream version can still be safe: Debian's published
fixed package is 1.6.48-1+deb13u6.

Without that assertion, an older found library produces a clear configure
error. PNG remains an optional format, with the existing explicit
`-Dpng=disabled` choice. Auto detection does not silently remove theme icons.
The API floor stays unchanged so a verified backport remains buildable.

This changes existing default builds. Both current FreeBSD repositories and the
OpenBSD ARM64 repository offer 1.6.58. The observed Ubuntu package has no
published matching fix proof. Those environments need a patched package
before enabling PNG; setting the assertion on an unpatched package is not a fix.
PNG themes and pixels are retained when the supported dependency is supplied.
No additional runtime library, decoder rewrite or bundled dependency is needed.

## Concrete candidate and checks

The reviewed two-file Meson candidate is saved at
`builddir/codex-2026-10-05/png-security-policy/candidate.patch`. It adds one
boolean option and one configure guard. Six isolated real-Meson policy cases
pass: auto/explicit old-package refusal, auto/explicit fixed-version acceptance,
explicit older backport acceptance and explicit PNG-disabled configuration.
The synthetic package metadata tests the guard; it cannot prove a vendor patch.
The user approved the patched baseline and the guard is applied to production Meson files. Real-dependency qualification is accepted; see the [baseline report](../verification/VERIFICATION-2026-10-05-PNG-BASELINE.md).

Installation documentation now records the approved guard. Verify real package identities and re-run relevant configure/build/runtime gates before closing qualification.
The assertion must remain off on the recorded unpatched/unverified targets.
Signed vendor updates are preferred; a private patched-library verification
profile can supplement an unavailable repository update, with its provenance
and different package scope recorded. Native Linux sanitizer CI remains a
separate release gate.

## Compatibility decision

[AGENTS.md](../../AGENTS.md) says: “A specific compatibility break requires an
explicit user decision.” This proposal deliberately refuses existing builds
that found an older PNG library without verified backport evidence. The user's
secure-default preference informs the recommendation but does not identify this
specific library/version behavior. The user explicitly approved the patched baseline on 2026-10-05. That selects the guard and assertion
policy; it does not certify all third-party dependencies.

Primary evidence: [libpng security baseline](https://www.libpng.org/pub/png/libpng.html),
[Debian fixed backport](https://security-tracker.debian.org/tracker/CVE-2026-46675)
and [Ubuntu evaluation status](https://ubuntu.com/security/CVE-2026-46675).
See the dated [package audit](../DEPENDENCY-AUDIT-2026-10-05.md) for target scope.
