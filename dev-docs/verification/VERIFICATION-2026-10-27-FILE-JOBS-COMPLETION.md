# File-job acceptance supplement — 2026-10-27

Owner: Cam; FILE-JOBS-01 [#68](https://github.com/gonz0w/fvwm-x/issues/68).
Base: merged PR289, `8d996ce9ac2f5578702e1299d6762be0b4d5c27e`.
Both previously missing frozen coverage rows now pass. Separate source,
archive, result and six-capture review supports #68 closure on the final
reviewed PR's merge. This supplement changes only the live-test driver, not the #68
runtime. [Original five-target evidence](VERIFICATION-2026-10-27-FILE-JOBS.md)
and its limits remain applicable; those gates were not repeated.

[Exact provenance and results](VERIFICATION-2026-10-27-FILE-JOBS-COMPLETION.json),
[unmodified screenshots](../../doc/development-preview.md#actual-copy-progress).
Canonical raw evidence is preserved in the owning local checkpoint
`builddir/codex-2026-10-27/r1-file-jobs-followup`.

## Linux ARM64 actual filesystem cases

The unchanged file-job package's static test executable was built with the
approved Go1.27.2 SDK, not the unqualified macOS SDK. All 568 Go/module inputs
were checked against merged PR289 and retained in an immutable source snapshot.
The ELF is AArch64 and has no interpreter/dynamic segment:
`7f80ac57318e1f7b2e0570128d8d91b6a012fae6530440f62fcad1c3332bfb2d`.

A private cached Ubuntu24.04 ARM64 container uses initial OCI-created writable
tmpfs mounts and a separately seeded read-only private volume. It adds no
capabilities, privileged mode, network, host bind or security relaxation and
does not mount/remount from inside the test process. Four exact cases pass
without skips: real cross-filesystem move/trash refusal, cross-filesystem
destination replacement, full-filesystem source preservation and read-only
filesystem refusal. The permission case separately passes with child UID/GID
65534 and cleared supplementary groups. Mount IDs, kernel/filesystem errors,
child identity, exact command and test-event records are retained.

All 25 exported payloads, modes, hashes, archive members and gzip CRC were
verified before the exact labelled containers and volume were removed.
Archive: `e9483f72d21ef2ac6f067e48c48c628f7c1d09c7ea7f375737ddc6aa9850f73f`.
This is Ubuntu userspace on the recorded Podman VM's Linux kernel under
emulation, not physical ARM64 or Ubuntu-kernel qualification.

Earlier attempts remain distinct: an unsupported mount option failed before
initialization; a crun memory-cgroup failure and a startup ENOSPC failure
occurred before any test child ran. The image does contain Python. Installed
Podman documentation and source showed its automatic tmpfs copy-up copied a
large image `/tmp`; the successful profile explicitly disables that copy-up
and uses a bounded 1 GiB memory cap. No test assertion was weakened. The
original denied in-container remount from PR289 remains recorded.

## Actual copy progress

The new `--progress-only` driver keeps the original eight-operation/lifetime
scenario unchanged. Each theme uses one private 128 MiB regular source file;
the observer is armed before Copy. It observes an actual new active job with
nonzero copied bytes, verifies the owned worker identity and partial output,
then briefly pauses that worker for a readable native screenshot. Native
browser input remains responsive. Native Cancel is followed immediately by
resume, explicit cancelled/partial results, actual child reap, unchanged source
hash, exact partial destination prefix and a successful follow-up operation.
An emergency resume guard and final cleanup prevent a suspended owned child.

Slate dark1.25x, Dune light1x and enlarged Classic2x all pass. Six unmodified
running/cancelled captures retain actual scale, binary, theme and config
identities. The explicit pause makes these functional observations, not
throughput measurements. The fixture payload is not archived.

The first enlarged Classic attempt ran concurrently with Dune and exhausted
the small guest filesystem before submitting its job. It remains a setup
failure. After verified obsolete evidence retirement, the unchanged Classic
driver passed serially; no product change or larger payload was used.
The canonical 118-file archive includes all three passes and that failure:
`3f56862da55ef7776b6e04aa508f93de969a785b7a1fd0755f0139dbde62d7e8`.
Guest evidence duplicates were retired only after exact archive verification.

## Acceptance limits

This closes the two frozen #68 coverage rows in the composed evidence. Separate
integrated PR source/evidence review is accepted. It does not qualify an entire release, physical hardware,
all filesystems, network mounts, arbitrary themes, clipboard integration or
the new pinboard. The original documented copy/move/trash race and metadata
limits remain. The five earlier target passes are reused by source identity;
new pinboard code requires its own affected checks.
