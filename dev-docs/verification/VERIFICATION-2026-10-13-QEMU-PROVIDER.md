# CI-01 owned QEMU provider — 2026-10-13

Status: affected runner scope locally qualified; fresh provider build, installed
consumer/default-X and actual remote CI adoption remain gated. No FVWM runtime,
configuration syntax or mandatory library change is claimed.

## Scope and ownership

The user approved replacing the inactive vmactions/AnyVM wrapper. This checkpoint
implements five related stories:

1. Strict consumption of fixed image/profile inputs and independently verified
   bootstrap keys/ARM firmware; no downloaded Python or launch script executes.
2. Fresh bounded disk/source/script snapshots and static four-profile QEMU
   arguments; no expanded/prepared image cache is reused.
3. Owned loopback SSH, generated job-key rotation, private host records, finite
   extraction/boot/command/log/evidence limits and joined/reaped teardown.
4. File-based job scripts with process stdin at EOF, retained failure evidence,
   cancellation cleanup and explicit disposable OpenBSD AMD64 VESA setup.
5. Inactive eight-job core/Go candidate wiring, distinct package preparation,
   pinned reviewed checkout/artifact actions and exclusion of private keys/disks
   from uploaded artifacts.

`tools/maintenance/qemu_bsd.py`, the small shared lock-loader extension in
`ci_assets.py`, `tests/test_qemu_bsd.py`, two owning Meson registrations, the
boot-input lock and candidate/scripts own this batch. No C/Go runtime source
changed. The final scoped archive contains 1512 regular files/thirteen owning
paths, derived from the accepted 1502-file `04c1` utility/XDG source. All other
accepted donor bytes/modes remain unchanged. The shared checkout's unrelated
parser/desktop work is excluded. The archive contains source, not VM key contents.

Evidence root: `builddir/codex-2026-10-13/qemu-provider/`.

- `owned-source-final.tgz` / `owned-source-final.json`: final source ledger.
- `policy-source.tgz` / `policy-source.json`: exact seven-file native Python scope.
  Full source SHA256: `52008da25fb9d55d70b475ac716e2c40f578df64edf68bb51e4cfbb7e990b953`.
  Policy SHA256: `9c50ff7f7fe96de9f2bebed6c6b38d5e1f48b05dccd49ef99e477f8764ac6547`.
- `*-owner-evidence.tgz`, `dispatch-owner.json`, `owner-policy-validation.json`:
  native runners, platform records, source archive and full logs.
- `acceptance.json`: final source, policy, candidate and actual control composition.

## Actual affected checks

| Check | Result |
| --- | --- |
| Linux/FreeBSD/OpenBSD AMD64/ARM64 |26 runner +23 asset units each: 294 native executions pass. Six independent evidence archives verify the same source and complete logs. |
| Actual pinned-image smoke |All four images boot, rotate authentication, transfer source, run a guest script, collect scoped evidence and terminate. These are local QEMU TCG results. |
| Final actual lifecycle controls |Six pass: same-size changed cache refusal before execution; invalid expanded image refusal before boot; bounded SSH/boot failure; guest exit 23 with retained evidence; run deadline; SIGTERM cancellation. Generated keys are removed and owned VM PIDs are reaped. |
| Final script-stdin fixture |A 20-KiB script executes from a file; a base-shell read sees EOF, following commands execute, and the expected artifact is collected. |
| Candidate model/syntax |Eight OS/architecture/profile combinations, reviewed action pins, read-only permissions, disabled checkout credentials and selective artifacts pass. Four shell recipes parse. Candidates remain inactive. |

The 26 runner units include actual subprocess output/deadline controls, hard-file
short-write refusal, separate binary stderr caps, console overflow, cleanup after
collector startup failure, key/status retention after cleanup failure, core-dump
disabling, source/member/geometry rejection and restricted VESA opt-in.

Finite limits: 64 GiB physical expansion, 256 GiB virtual geometry, 64 MiB log/console
sinks, 2 GiB collected evidence, 256 MiB compressed source, 128 MiB per source member,
4 GiB aggregate source and 100000 members. Boot/run overrides remain finite.
Collector limits cover writes as well as parent observations. Resource limits
are applied in a fresh exec worker, not `preexec_fn` in a threaded parent.

The 13 original assets retain their accepted download/offline-copy evidence.
All five additional published bootstrap/firmware inputs were downloaded and
compared with publisher size/digest receipts; their locked bytes are checked again
on cache hits before use. Original image/profile pins are unchanged.

## Default-server startup profiles

These checks reuse the accepted Python harness's private-cookie authority,
finite readiness and owned teardown; they do not run a new installed WM/client.

| Provider | Actual backend | Probe |
| --- | --- | --- |
| FreeBSD 15.1 AMD64 |BIOS standard VGA / VESA |1920 x 1080; authenticated RandR query and cookie-less refusal pass. |
| FreeBSD 15.1 ARM64 |Pinned ARM firmware / ramfb / scfb |800 x 600; both authority checks pass. |
| OpenBSD 7.9 AMD64 |BIOS standard VGA / VESA, explicit disposable aperture 2 fixture |1920 x 1200; clean two-boot setup, verified aperture value and both authority checks pass. |
| OpenBSD 7.9 ARM64 |Pinned ARM firmware / ramfb / wsfb |800 x 600; normal package acquisition/file-based job and both authority checks pass. |

The OpenBSD AMD64 image defaults to aperture 0. Its VESA log fails int10 access;
VirtIO VGA still binds the kernel VGA console. Non-VGA VirtIO GPU reports
“no matching child driver; not configured” in this pinned AMD64 kernel. Serial
boot can start that image with the non-VGA device but cannot provide a supported
framebuffer. No kernel rebuild or additional graphics dependency was inferred.

`--openbsd-vga-test` is an explicit CI fixture option restricted to OpenBSD
AMD64. It sets aperture 2 only in the owned disposable guest, shuts down cleanly,
reboots and checks the value before the X probe. The inactive candidate opts in
only for that Go/default-X job. Ordinary/core jobs retain the image default;
FVWM and host security defaults do not change. OpenBSD documents that mode 2
permits int10 memory access, including part of guest kernel memory; this setting
is not a production-hardening recommendation. [OpenBSD xf86 manual](https://man.openbsd.org/xf86.4).

Actual server logs, RandR output, image/source hashes and guest archives are
validated in `acceptance.json`. OpenBSD verification storage is on `/home`, with
the conventional `/root/fvwm-ci` entry retained as an owned guest symlink.

## Retained failures and corrections

- Initial 64-GiB virtual geometry refused the actual 200–206-GiB sparse images.
  Physical expansion remains capped at 64 GiB; separately bounded 256-GiB virtual
  geometry accepts the reviewed standalone images. All failed images/logs remain.
- An actual short-write unit exposed a producer exiting successfully at exactly
  its file limit. Full sinks now refuse success; capped pipe collectors cover
  binary diagnostics/console independently.
- Initial OpenBSD native extraction required explicit `tar xzf`. A slow AMD64
  interpreter startup preceded a fixture PID-file write. The two-second deadline
  still covers startup; the fixture now captures the real owned PID at launch.
- Podman transport stops/launcher behavior and retries remain logged; final
  Linux ARM64 qualification uses the existing container without configuration
  replacement. An “already running” launcher result is retained too.
- OpenBSD package acquisition produced DNS and truncated archive failures.
  Complete host-observed signed Meson archives parse and install through normal
  guest signature checks; ARM disk/limit evidence records ample capacity.
  File-based job execution separately fixes inherited-script stdin. The final
  ordinary ARM acquisition/X probe passes. The earlier truncation cause remains
  unestablished; neither signature checks nor trust settings were relaxed.
- VESA aperture denial, unsupported non-VGA startup/driver controls, and an
  initial restart boot-log name collision are retained. Reboot readiness uses a
  distinct log prefix. Failed alternative hardware trials are not shipped modes.
- The first stdin fixture required Perl absent from the minimal FreeBSD image.
  The final base-shell fixture passes unchanged production code.

Thirty-six owned job records finish as passed or failed; no generated job key or
pending owned job remains. Failed/replaced evidence is retained. No older guest,
other chat's disk/build, publishing credential or upstream message was modified.

## Adoption gates / external decision

Both new FreeBSD provider package inventories report libpng 1.6.58. The approved
PNG baseline is 1.6.59 or a verified vendor security backport. Candidate preparation
records the actual version and does not assert a backport; a fresh PNG-enabled
build cannot be accepted with those packages.

The [patched PNG input proposal](../decisions/CI-PNG-INPUT-PROPOSAL-2026-10-13.md)
requests the separate external CI delivery choice: verified pinned source
bootstrap with upstream tests/actual loader identities, or verified updated
vendor packages. The source bootstrap is not activated by this checkpoint.

Fresh standard/Go builds, staged identities and actual installed default-X
consumers on the new provider images remain required before workflow adoption.
The existing accepted utility/XDG and local old-provider gates remain valid;
startup probes do not substitute for these new-image gates. There is no Git
remote for actual Actions execution. Native Linux release sanitizer CI, inherited
container contents, physical GPU/input and publication retain separate owners.
