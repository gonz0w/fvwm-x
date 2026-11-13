# AMD64 docking build, install and X qualification

Issue [#222](https://github.com/gonz0w/fvwm-x/issues/222) uses source
`02b3548530079c6cbfba2f890bdcc0e6ad87b9b2` and tree
`c61116969a51986097969b6a2d766a7107c64591`. This is the canonical
merge of the ARM64 checkpoint, with the reviewed tree preserved.
Reuse the [ARM64 installed runs](VERIFICATION-2026-11-11-DOCKING-NATIVE.md)
and [theme and cross-build runs](VERIFICATION-2026-11-11-DOCKING-QUALIFICATION.md).
The AMD64 guests execute x86-64 code under QEMU TCG. These results establish
guest behavior under CPU emulation.

## FreeBSD 15.1-RELEASE-p4

The reviewed plan `de936ad7b4cea3c886f2945ce3de606e805d8bf5eccf4b55df0d916e78a6a629`
reused the retained compiler, private Go and PNG, and Meson build in an
isolated child with private firmware. It built 37 install targets and staged
the product in a private DESTDIR. The installed core version probe passed.

`tests/run_bsd_default_x.py --disposable-vm --desktop-docking-only
--execution-profile emulated-cpu` ran the staged program and data on the
default X server. The server refused an unauthenticated client. All five
cases passed, covering private profile save, read-only preview, stale
workspace refusal, preservation of a newer manual window move, and
restoration of workspace, panel and window placement. The scene created
no screenshots. QEMU exited normally and the backing chain was unchanged.

The raw result SHA-256 is
`10dc01dc8f0b01875c98aff853f027b66c817f7108ff8098a32549e9e190d653`.
The raw archive SHA-256 is
`abdf9080faab00f25f5316bc69721d0089ba89c51b5fe9cea73648b464f22b02`.
The scene evidence SHA-256 is
`2267e210eb740d3308ed1f973717a74573350e49132ff5ffb5550add0cc57e00`.

## Debian 13

The reviewed plan `611a8c8d3a66c99c9ac0e218676abfa5913a7d0af436aca99bd7d07a2359b29a`
built 37 install targets from the retained Meson build, staged them privately,
and passed the staged core version probe. The staged program and data passed
all nine docking cases on isolated Xorg dummy, with screenshots disabled.
QEMU exited normally and the backing chain was unchanged. This is supplemental
Linux evidence. The Ubuntu 24.04 AMD64 release profile remains open.

The raw result SHA-256 is
`5ccfaf86f8902c088f450a403e501a8d4d292b4e2f2e7317e95ab440f7314c40`.
The raw archive SHA-256 is
`f1c244b9eafdd5c91424f43cc1b27558c247c7f561fa47eaefa76e82a6b3d713`.
The scene evidence SHA-256 is
`4a2c1c8ce6f42bb82114eba98c49e43e461a5ac8fd4004dae5d1731598d335b7`.

## OpenBSD 8.0

The stock-image probe used reviewed plan
`f4058453f7531a21ec77d4961812dc0c2cd7c54e057273117bcc340effb0a64d`.
Boot, guest authentication and input transfer completed. The guest script
reached its 1,500-second deadline before returning an evidence archive.
QEMU exited normally. Build, install and X results are unavailable and
the timeout does not identify a product defect. The stock image probe
did not enable the separately documented legacy VGA test setting.

The raw status SHA-256 is
`caa7ea872e17925e963bd74c135f1d8db6016e5d1867f429397e523e46fc06a5`.
A subsequent owned run needs separate preparation and build progress
records so a timeout identifies its phase.

## Physical docking

A real dock and two displays at different scales are required to verify
unplug and replug during a prepared restore, connector re-identification
after restart, and whether the panel, windows and pointer remain reachable
on the physical outputs. The earlier dummy-output runs cover simulated
topology changes. This batch makes no physical hardware claim.
