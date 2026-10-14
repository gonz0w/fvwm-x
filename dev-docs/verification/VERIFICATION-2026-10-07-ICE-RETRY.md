# ICE callback ownership across setup retries

Status: focused Linux normal/scoped sanitizer and both native BSD ARM64 checks
pass. The repair joins the five-story descriptor checkpoint. Existing FVWM
configuration files remain usable; no library or option is added.

Repeated core/library setup previously saved the installed handler as its own
previous callback. A shared small policy now retains the earlier callback when
the candidate is the handler itself. Default-handler reset and replacement by
another callback keep their existing behavior. This closes direct self-delegation
on repeated installation; it does not claim to prevent all third-party callback
cycles or qualify broader session retry cleanup.

Bounded prior-source controls use the actual ICE setter with three core setup
attempts and a controlled session-open refusal. The first installed handler
delegates once; subsequent controls observe the self-reference without invoking
that old callback chain. Both normal and scoped sanitizer controls confirm the
identity result. No old recursive callback is executed.

The final pure policy unit checks repeated installation, defaults, replacement
and null candidates. Library controls force registration refusal over three
attempts; actual-core constructor controls force session-open refusal. Each final
installed handler delegates to the earlier callback exactly once. Wrapper
prototypes match the installed SM headers. Final Linux normal and scoped Clang
ASan/UBSan profiles pass seventeen pure and twenty-five actual-core checks each;
pure units enable leak checking and core checks retain the established scoped
leak-disabled policy. Both BSD ARM64 overlays pass the unit, real session/watch
controls and expanded constructor checks. The runner verifies actual Ninja object
replacement before linking.

Four complete archives validate payload hashes/modes and compressed EOF: 24 Linux
focused files, 125 per BSD overlay and 16 prior-source evidence files. Initial
wrapper declarations/results are preserved. Native overlay header-search and
binary/artifact-path collision diagnostics remain retained; both runner issues
were corrected without production changes or deadline extensions.

Frozen source: 1349 files, six owned paths from descriptor1347. Archive SHA-256:
`b08cb75a9c1dc007ed86b1aca1508a4c02f03643a27510b59f43eef1e4e16e87`.
Evidence: `builddir/codex-2026-10-07/ice-retry-production`.
Combined matrix qualification and native Linux release CI/physical evidence
remain pending. Broader registration/retry lifecycle and high-descriptor live
admission remain separate work.

Combined local qualification now passes under descriptor1349: 1336 standard
entries/eight profiles, 120 staged identities, 132 default BSD X drivers,
optional 172/152 and 55 scoped checks (22 pure/33 core). Seven archives validate.
The [descriptor report](VERIFICATION-2026-10-07-MODULE-DESCRIPTORS.md) retains
the original OpenBSD display refusal and Linux AMD64 Xorg teardown failure with
three unchanged affected passes each. Native release CI/physical evidence remains
separate; this does not claim every initial run passed.
