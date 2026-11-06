# OpenBSD ARM64 drawing and layout observation

This slice of [#211](https://github.com/gonz0w/fvwm-x/issues/211) and
[#212](https://github.com/gonz0w/fvwm-x/issues/212) exercises the
[shared drawing and layout fixture](VERIFICATION-2026-11-05-DRAWING-LAYOUT-OBSERVATION.md)
on stock OpenBSD 8.0 ARM64 under QEMU HVF. It uses private Xvfb, Slate dark,
ordinary drawing and the private native Buttons layout. FreeBSD and Linux
evidence retain their original source and profile scope.

## First attempt and repair

The first attempt compiled the four selected targets in 16.93 seconds.
The fixture collected four action samples, two resizes and all eight CPU
and RSS observations. It failed before requesting module exit. OpenBSD
returned `EPERM` when the harness queried the process group of an owned
module in the core's separate session. The guest runner collected the
failure evidence and QEMU exited with status zero.

The repair changes the private BSD fixture's exit predicate to check the
bound PID with signal zero. It sends no signal, and only a missing process
counts as retired. Ownership binding, process sampling and the
`KillModule` requests retain their existing checks.
[OpenBSD process-group query](https://man.openbsd.org/OpenBSD-8.0/getpgid.2) ·
[OpenBSD PID existence check](https://man.openbsd.org/OpenBSD-8.0/kill.2)

Initial source revision `8ae512a6ea5c260ef0bb9597c105dac72dc0e1f1`,
tree `2baec768a98b8db4f6827857f2c40f0b9b9b00f6`.
Initial plan SHA-256 `b532788c0224900e235b84c6ad30e0396fc1b8311a40d498ae09fd8dd634078e`.
Failed fixture SHA-256 `2081d1c5e8c6651d3eae2563902a36a2c7dcde5f79a6832480fa74997af106c5`.

The reviewed repair source tree overlays the fixture file on that revision.
Tree `28c0250073d9506b790020054f6647c83ad69b9b`.
Repair plan SHA-256 `63b0095de12c418d7a1e7d16e0646c5c39ffdd94f59548ab41779f8358023ddf`.

## Successful OpenBSD run

The retry configured in 7.68 seconds and compiled the four selected targets
in 17.02 seconds using OpenBSD Clang 22.1.6, Go 1.27.2 and private PNG 1.6.59.
All four executable hashes matched the first run. The fixture passed in
2.66 seconds with four action samples, two resizes and all eight process
samples. Both owned modules were present before their exit requests and
retired before the core exited successfully.

| Module | Workload CPU before / after | Idle CPU before / after | RSS throughout |
| --- | --- | --- | ---: |
| Buttons | 0:00.01 / 0:00.02 | 0:00.02 / 0:00.02 | 6,436 KiB |
| Form | 0:00.01 / 0:00.01 | 0:00.01 / 0:00.01 | 3,764 KiB |

These are displayed cumulative CPU and RSS snapshots. The CPU counter
has centisecond resolution. High-water memory, wakeups and implicit
roundtrips remain unavailable. This sample qualifies the fixture on this
profile. A paired run is still required to compare rendering or layout costs.

## Executable and local dependency records

| Executable | File bytes | Resolved `rlib` rows |
| --- | ---: | ---: |
| Core | 5,345,560 | 23 |
| FvwmMFL | 436,888 | 3 |
| FvwmForm | 1,772,360 | 22 |
| FvwmButtons native layout fixture | 1,750,688 | 23 |

Each `ldd` record also names `/usr/libexec/ld.so`. The 41-entry local package
list matched across both attempts. It includes Cairo 1.18.6, D-Bus
1.16.2p1v0 and librsvg 2.63.2v0. The retained records identify the selected
dependencies and executable sizes. Loader file size and hash, incremental
installed cost and remaining platform costs still need separate evidence.

## Evidence and continuation

The retry verified 3,609 source entries before and after the build.
Source manifest SHA-256 `f6437d2c6a8f70dd14a8827d69bc6b7e570bb8033509fa40e5890944ae7b7d0b`.
Fixture result SHA-256 `62eda81019f99f6ddd8905e527c52a048c1549037b293babbf22799fe9f96433`.
Xvfb SHA-256 `dcc3ccec84c74057ff837dd5dda90cd8627fb7e5ac5fd13e7d9256d7bdcd5964`.
Base `ps` SHA-256 `6819e5e2f29a5656792ad7383f337124e304ed9d0d40a8250d60483003cc73bb`.
The 26 focused host graphics tests passed.

The artifacts are under
`builddir/codex-2026-11-06/r1-openbsd-drawing-layout`.
Both 48-file exports matched their recorded hashes, every owned command
group retired and both QEMU runners exited with status zero during teardown.
The first export and diagnostics remain. The first disk was retired after
the successful retry, freeing 3,685,548,032 allocated bytes. The current
disk is stopped. The successful export is `repair01/openbsd-arm64-export01`.

OpenBSD AMD64, paired runtime measurements, implicit waits, remaining costs
and the broader consumer and release gates are still open. This batch adds
no new UI and uses the existing Slate dark fixture. It adds no install,
theme replay or periodic whole-WM run.
