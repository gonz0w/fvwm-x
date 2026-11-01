# FileManager1 installed-service closeout

FILE-INTEGRATION-01 #67 remains open. The selected closeout covers eight enabled
service groups on each of six targets, two disabled groups on one ARM target per
OS, and one Linux ARM service-to-`--file-transfer` observation: at most 55
groups. Linux ARM passed all 11 applicable groups; FreeBSD ARM and Linux AMD64 each
passed eight enabled groups; FreeBSD AMD64 passed eight, FreeBSD ARM
disabled passed two, and both OpenBSD enabled runs passed eight each: 53 of
55 passed. The two OpenBSD ARM disabled groups remain unrun. Final review and
integration remain pending, and whole
SELECTION-01 #22 remains open.

The [transfer report](VERIFICATION-2026-10-31-FILE-TRANSFER.md) and retained
PR315 evidence cover their own source and scope; historical passes are not fresh
service executions. The [compact receipts](VERIFICATION-2026-10-31-FILEMANAGER-INSTALLED.json)
identify the verified archive, member hashes, actual observations and commands.
The original archive remains in the owner-controlled
`builddir/codex-2026-10-31/r1-file-transfer/evidence` directory.

## Linux ARM64 development run

Source34 closure is
`7708bcbf4f45e31bb93cb9f20a140f30922be5e244ff9595699cfdfdb491d6ec`;
source archive SHA-256 is
`9726801aa8963c300e9cc6076289500e69a204511ba17e50b6503ee455c9550d`,
and the fixture SHA-256 is
`c0dbf3745333b34bfedb480259afb5a74bce650a86d9c951a14a280d95e8d4c9`.
The service01 archive hash is
`76926208c0e828e673a9791b35a1e0fdaabdc217a87e9d52c915b38150ccaad5`;
all 179 member bytes were verified. The run used Ubuntu 24.04 ARM64 native CPU
under AppleHV, with an enabled installed-service profile and a software no-bus
adapter. This is not native AMD64 or physical-hardware evidence.

The enabled run passed E1–E8: installed owner and direct-parent identity;
independent ShowFolders; repeated provider reuse and deiconification; two-parent
ShowItems including literal and byte names; native properties summary; competing
owner refusal; unset and nonexistent bus without autolaunch; and service Wait,
name release, browser continuity and restart. E9 separately passed the
startup-only file-transfer option propagation observation. This does not repeat
the transfer protocol qualification.

The disabled run passed explicit installed-service refusal and ordinary installed
browser use without a bus. It borrowed the enabled run's qualified core, MFL and
Form actors. The enabled preflight had separately built the selected native bus,
policy and session artifacts, run the policy check and installed the session-startup
tag; that preflight was reused, not rerun as a new Go or policy suite. Actor
identities and profile metadata are retained in the receipts.

Cleanup failures were empty. Owned Xvfb, private bus and core Waits returned zero;
actual native module Waits are recorded. The Go-owned native service helper's
exit status was not independently observed: PID retirement/absence is not a
helper Wait result. The owned Linux hold cleanup is retained separately.

## FreeBSD 15.1 ARM64 enabled run

The actual enabled run passed all eight groups on native CPU under HVF. The
verified inspect archive SHA-256 is
`1ccf0ea8896936ce200a908b1a3e4b8c3989dc6c4f4c3a141f2f491a38b8b3ac`
(19 members); the execute archive is
`33e7560a4ef0cc07e896fde0a1966d6b7516ea2de554dd2e2f7fd84c3644933e`
(116 members, 603,341 expanded bytes). All members were verified. The source29
to source34 overlay contains the transfer clock, tests, manual, documentation
and new fixture; the prior native transfer binary was preserved and unused by
this service run. The enabled bus and policy were built from current source,
one C policy ran, session Go and browser manual were required tag companions,
and five actors were unchanged. Inspect and execute are separate receipts.

Actual core, X server and private bus Waits returned zero. Read-only browser PID
absence is not a browser Wait; helper exit status was unavailable. The disabled two-group continuation is recorded below. The lab shut down normally; its cleanup receipt is retained.

## Linux AMD64 enabled run

All eight enabled groups passed on emulated-CPU Debian 13, glibc 2.41 and
kernel 6.12.113; this was not Ubuntu or native AMD64 CPU execution. The inspect
archive SHA-256 is
`08ced355c3765a37a86113a287603abe17d799411c211a12b1510a6d0c657b87`
(19 verified members); the execute archive is
`6be1b6ce408e73418ab5979de4759f672c290f07cccf4a6cd73c49ffdd538cfb`
(116 verified members, 603,663 expanded bytes). Its plan SHA-256 is
`546100fd7a96f136ad721a9bbe38eb7b579517821eb225eb586156f859feadb0`.

The source29-to-34 inputs and older unused transfer binary follow the same
boundary as the FreeBSD run. Current bus and policy were built and the policy
ran once; session Go, manual and two tags were required, with five other actors
unchanged. Initially missing policy binary and objects were within the selected
12 native outputs. A lead host assertion expecting no policy rebuild was overly
narrow and corrected before acceptance; it was not a product failure or an
extra test. No new Go, race, transfer or theme suite ran.

Cleanup failures were empty, and X server, core and private bus Waits were zero.
Browser PIDs were absent; their Waits were not observed. The hold shut down normally; its cleanup receipt is retained.

## FreeBSD AMD64 enabled run

All eight enabled groups passed on FreeBSD 15.1-RELEASE-p4 with emulated AMD64
CPU. Inspect archive SHA-256 is
`749e47832143a2f3fc46ca4f43ff99b791e72fb44980b94ad40781e226ab134e`
(19 members, 205,246 expanded bytes); execute is
`581cae779709b622f62cbbf1acb433514cbac1fb23430702c7dc9991cd2b1bde`
(116 members, 621,919 expanded bytes). All members were verified. The plan
SHA-256 is `9dcb9a95dadfdc157f782dfa18abaa403cfaed514d0092a4774ef00f0664e676`.
Source32-to-34 changes are test and documentation only; the current transfer
helper was preserved and unused by E1–E8. Build, one policy run and session
companion ran with five actors unchanged. Cleanup failures were empty; X server,
core and bus Waits were zero, and browser PIDs were absent. Normal poweroff completed and its cleanup receipt is retained.

## FreeBSD ARM disabled inspection

The disabled inspection stopped at its three-option contract before build,
install, tests or any group. Its verified ten-member archive SHA-256 is
`5196253d5b4f7c4593a43db81419ad050e5cb8232fbfb6aac24f13f7b457556f`.
The new prefix derived `localstatedir=var` and `sharedstatedir=com`, whereas
the retained enabled values were `/var/local` and `/var/local/lib`. The lead
selected reconfiguration of only this new unbuilt profile with the two retained
directory settings, followed by resumed inspection. This was not a product change, waiver or enabled rerun; the option recovery
receipt records the two directory settings passing.

A second inspection also stopped before build or test, with zero groups. Its
ten-member archive SHA-256 is
`353c4308da9301c2d2d68c24545eb929da72179d863f58ca122f2da4bf0b34ba`.
The enabled cache symlink resolved outside the selected root to
`/root/fvwm-go-baseline-20261014/go-cache`: same UID 0, mode 755, pinned SDK
and an exclusive nonconcurrent VM. The lead explicitly accepted reuse through a
separate private FreeBSD-only gate (SHA-256
`eee4a40a904823efd720bd164a4541a379f3de2ccbb13b00580f4de7769bc2a8`);
its narrow cache guard review was clear. The original OpenBSD disabled gate was
unchanged. Neither inspection failure is a functional service failure.

The fresh inspect03 archive
`d485f3385e592006dd846d8d04d0041bb4483581b76c4a66b99a6aaa2aa6d7c5`
contains 19 verified members. Execute03
`8a0ad02a513c1bae123a3b52ba5bdc3c8156a3393a6616ef29c0f96069a1e2b3`
contains 58 verified members and passed both disabled groups with source34
post-check, no cleanup failures and actual X, core and bus Wait 0. Qualified
enabled native actors were preserved. Its plan SHA-256 is
`1c4d7b64236bfa88edb975a756896ec48ceea2abab589717c2868990a3638cb2`.
The 111 build commands were 102 static libfvwm3 prerequisites, seven leaf
commands and two manuals, limited to four selected leaves and two manuals.
No all-build, Go, C policy or transfer suite ran; the owned SDK baseline cache
was reused without copying it.

## OpenBSD preparation failures

Both PREPARE01 runs stopped before any service group. They could not find the stale top-level cache
receipt `builddir-bsd/provider-dbus.txt`; the source32 receipt was nested in
transfer-continuation preparation. These are preparation failures, not service
behavior failures. Adapter v2 repaired the cached receipt lookups;
no source, gate or case changed. The later v2 runs are recorded below.

The verified AMD64 archive
`a667947b980dfc23660555dae33815531ab26db3dbeeda9e6355802496c809d2`
contains 71 members; ARM64 archive
`3628e66727d9ad09416fdfb2517d319fd75951ec6c9e9d671482074d5a35b94c`
contains 23. Member hashes, sizes and modes are retained. Both runners had QEMU
Wait 0 through SIGTERM cleanup, without a normal guest-poweroff claim. After
logs were verified and `lsof` found no users, only the failed clone disks were
retired: 4,202,889,216 ARM64 and 4,169,400,320 AMD64 logical/allocated bytes.
Original compiled payloads and base assets remain. No owned labs remain active.
Linux AMD64, FreeBSD AMD64 and FreeBSD ARM completed normal shutdown with QEMU
Wait 0; Linux ARM's Podman container and machine stops also returned zero.

## OpenBSD service02 continuation

Both OpenBSD 8.0 enabled runs passed eight groups with zero cleanup failures
and source34 postguard. ARM64 used native CPU under HVF; AMD64 used emulated TCG.
ARM's aggregate job failed the disabled inspection before build or D1/D2:
`licensedir` was empty on the retained enabled profile but `.` on the new
profile after directory defaults were copied. Its two disabled groups were not
run. AMD's job passed. Both QEMU Waits were zero through SIGTERM cleanup; normal
guest poweroff is not claimed and no owned labs are active.

ARM's compact archive SHA-256 is
`eb754f179eb220bcd4f0f63fa1e59a800584e5c053c85f5b818fceec81e893de`
(154 current and 11 host members). Its full archive is
`cb8c2ce87d764f8db746a467ecad729ab86aac738af6717722491ed00b5430f0`
(4,682 members). AMD's compact archive is
`0b6574b439e92affae1f50aadc9c3a4586ff761f189d3ecbd513d4bcaa37fd65`
(152 current and 11 host members), and full archive is
`9375b98caf674b924fafcab2cf6776fc8f55be573377b20eaaa1d615efb3e691`
(4,608 members). Full member manifests and export-verification receipts are
retained; compact member bytes and hashes were checked.

OpenBSD `head` rejected `-c`, so the v2 controller's ready reader silently
reported not-ready. The lead fetched exact ready, plan and input bytes with a
bounded Python handoff, reviewed the actual dry plan and atomically uploaded
the matching frozen approval object. The guest still checked approval, hashes
and re-inspected plan equality. The compact archives retain the handoff and
actual guest approval. The outer controller's empty approval list reflects its
broken reader, not an absent actual approval; this is not a human or automatic
approval claim. A reviewer P3 whole-template equality correction preceded the
AMD handoff; the first ARM object was already correct. No product, source,
test, gate or deadline changed.

## Remaining acceptance

The two OpenBSD ARM disabled groups remain unrun. The identified future setup
fix is to skip empty directory values in the new profile, then qualify only
D1/D2 at the next major or shared checkpoint under the updated
[workflow cadence](../WORKFLOW.md). This gap does not block implementation
merge under that cadence, but remains pending qualification and #67 stays open.
No further test jobs are selected now. Final delivery review, signature and merge
remain pending; whole #22 also remains open. Do not claim all 55 groups or whole
issue acceptance from the 53 observed passes.
