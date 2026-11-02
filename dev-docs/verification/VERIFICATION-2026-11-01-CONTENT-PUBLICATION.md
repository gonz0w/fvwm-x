# Content producer publication — source checkpoint

Owner Cam; #63/#74 with #71 cancellation; base merged PR332
`8a144fc9615e192517b1f59466a296a4fd343c53`. This is internal policy and
non-installed candidate preparation, not clipboard capture or allocator adoption.
[Exact checks](VERIFICATION-2026-11-01-CONTENT-PUBLICATION.json) retain source,
compiler, flags, dependencies, command exits, timings and artifact identities.

## Behavior and coverage

Four related stories share one source checkpoint:

- Reserved producer access requires exact owner/item/generation, outstanding
  work, current time, enabled/unpaused policy and representation-specific checked
  ranges. It neither creates a reader lease nor changes resource charges.
- Successful completion requires actual initialized lengths for every
  representation. All lengths validate before any published descriptor changes.
  Malformed success retires while draining work once. Full reservation charges
  stay until actual backend cleanup; a smaller published prefix does not release
  memory that an in-flight producer or reader still retains.
- Both candidate builds write a contiguous synthetic prefix while reserved,
  close writable access, publish its actual length and read only that prefix
  through a live lease. Readers cannot acquire reserved content. The metadata
  attestation cannot prove writes or detect holes; the native owner tracks them.
- `--cancel-producer` retires after the first bounded chunk, rejects subsequent
  access and late/duplicate publication, then completes normal wipe/cleanup.
  It does not grant a reader lease or revive a retired generation.

The authored policy cases cover exact/empty/overflowing bounds, unknown IDs,
foreign generations, pause/disable, expiry and clock rollback; multiple actual
representation lengths including zero, invalid later entries without partial
publication, duplicate/retired completion and retained cleanup charges. Existing
callers migrate together; no unsized-success API remains. No configuration
contract, installed command, feature default or dependency changes.

## Observed checks and limits

Five strict macOS ARM64 Apple Clang 21.0.0 compile/link commands pass with C11,
`-Wall -Wextra -Werror -pedantic`, assertions enabled and the explicit minimal
feature-independent header. Policy, policy-test and base/libsodium probe binaries
are recorded. Existing libsodium 1.0.22 pkg-config inputs are used; no package
was installed. Source hashes match before and after compilation.

One cheap pure policy executable passes on macOS ARM64 in 0.189 seconds. This is
portable metadata/arithmetic coverage on that host, not native Linux/BSD or
memory-protection qualification. Neither memory candidate ran. Shared native
policy/protection/refusal/cost comparison remains pending after assembly;
previous content-policy passes do not qualify changed source automatically.
Regional dump controls, actual mapping/locking, genuine unprivileged lock refusal,
optimized wiping and configured dependency closure require their scoped evidence.
No protection/adoption or end-to-end clipboard claim follows from compilation.

Repository/link and whitespace checks pass. Separate review remains required for
this source candidate and its signed delivery. No full WM/install/default-X/theme
suite runs for private source preparation, and there is no new UI to screenshot.
The existing session-only history, sensitive-item exclusion and persistence-off
choices remain; installed capacities and capture/PRIMARY/lock/logout decisions
remain distinct. #63/#71/#74 and the R1 milestone stay open.
