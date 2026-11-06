# FreeBSD ARM64 drawing and layout observation

One positive run starts the shared BSD checkpoint for [#211](https://github.com/gonz0w/fvwm-x/issues/211)
and [#212](https://github.com/gonz0w/fvwm-x/issues/212). The
[G7 fixture](VERIFICATION-2026-11-05-DRAWING-LAYOUT-OBSERVATION.md) collected
all required samples and retired both module children. The same guest supplied
loader and package metadata for the [G6 costs](VERIFICATION-2026-11-04-DRAWING-LAYOUT-COSTS.md).

## Positive run

FreeBSD 15.1-RELEASE-p4 ARM64 ran on a hardware-accelerated virtual CPU.
The fixture ran as the isolated guest root with its own Xvfb and a private
module path. It used Slate dark, ordinary drawing and the private native
layout control. The core and FvwmMFL built incrementally in 5.82 seconds.
Buttons and Form reused their verified G6 binaries. The fixture ran four
action samples and two resizes in 2.46 seconds.

| Module | Workload CPU before / after | Idle CPU before / after | RSS throughout |
| --- | --- | --- | ---: |
| Buttons | 0:00.01 / 0:00.01 | 0:00.01 / 0:00.01 | 10,044 KiB |
| Form | 0:00.00 / 0:00.00 | 0:00.00 / 0:00.00 | 9,544 KiB |

The displayed CPU values stayed steady at centisecond resolution during the
sampled intervals. Wakeups and peak memory remain unavailable. The run
qualifies the collection path for this profile. Paired comparison timings
and broader runtime acceptance require their own samples.

## Loader and installed package records

The eight executable files and 32 resolved library files matched the G6
hashes. The ELF program headers identify one interpreter for each binary.
The shared interpreter is `/libexec/ld-elf.so.1` at 116,920 bytes.
SHA-256 is `e1d41e22b5b3a7dd9e21fd0dfac096601b01bd5c23c1ea87b59a0963ebc1f753`.
These sums extend the G6 resolved library file lengths with that interpreter.

| Module | G6 resolved library bytes | Including interpreter |
| --- | ---: | ---: |
| FvwmButtons | 8,349,848 | 8,466,768 |
| FvwmButtons-cairo-eval | 10,300,384 | 10,417,304 |
| FvwmButtons-clay-layout-eval | 8,349,848 | 8,466,768 |
| FvwmButtons-native-eval | 8,349,848 | 8,466,768 |
| FvwmButtons-native-layout-eval | 8,349,848 | 8,466,768 |
| FvwmForm | 8,556,448 | 8,673,368 |
| FvwmForm-cairo-eval | 10,506,984 | 10,623,904 |
| FvwmForm-native-eval | 8,556,448 | 8,673,368 |

The local package database returned 596 entries with `pkg query -a` and
the byte-size field `%sb`. The selected records give exact nominal flat
sizes for whole installed packages.

| Package | Version | Flat bytes |
| --- | --- | ---: |
| cairo | 1.18.2,3 | 3,950,592 |
| pixman | 0.46.4 | 759,484 |
| libxcb | 1.17.0 | 5,243,924 |

The G7 license labels still apply to these package versions. Incremental
installed cost, unresolved owner lookups and AMD64 loader and manifest
measurements remain pending.

## Evidence and continuation

Source revision `5dfdb9e60c3f1fa50bfc05e072fc11a0ddea4132`, tree `234152bdfa4a4b96457b14eb71475447f421a73f`.
Source manifest SHA-256 `d944da4642930bc04b3e742a7d0a4ec8cb268a6eabe425adfb939b04535217c0`.
Fixture result SHA-256 `609d5220bdb8f7739d33aacbafe161c3aa2e370595a6178cce3dc7d864d8b9fe`.
Package manifest SHA-256 `d25aa296422ca4710f09ca6ddf15f0c97d6040a482789765239e9174a55cbff0`.
Reviewed dispatch plan SHA-256 `cae172cadeb118373b0fb2aa64099eb897614080903180f253a8752e162c73fc`.

The first attempt stopped before compilation. Meson found both an executable
and a static library named `fvwm3`. The single reviewed retry selected
`fvwm3:executable`. Both guests exited normally, and both backing images
kept their recorded identities. The failed image was retired after the
positive pass, freeing 168,427,520 allocated bytes. Its logs and receipts
remain alongside the successful stopped child.

Current artifacts are under
`builddir/codex-2026-11-05/r1-drawing-layout-qualification`.
The successful export is `repair01/freebsd-arm64-run01/export/g8-drawing-layout01`.
The report retains one positive Xvfb fixture, one incremental build and local
metadata collection. The fixture result and nested native receipt establish
the positive count of one. The outer runner still carries its inherited GUI
counter of zero. The 32 G7 host tests retain their previous source scope.

OpenBSD 8.0 still needs its first positive observation. The roundtrip work
first needs a controlled count of synchronous reply retrievals for one
connection. A useful feasibility probe has a prewarmed async zero control,
known Xlib and XCB retrievals, nested-call deduplication and an unrelated
connection control. Coverage of library-internal waits and actual blocked
waits remains a separate qualification requirement.

Both issues remain open for paired runtime measurements, remaining cost
evidence, implicit waits and broader consumer and release acceptance.
