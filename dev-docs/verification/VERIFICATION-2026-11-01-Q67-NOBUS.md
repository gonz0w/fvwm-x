# Q67 OpenBSD no-bus acceptance

Owner: Cam; FILE-INTEGRATION-01 #67. The two outstanding OpenBSD 8.0 ARM64
disabled-profile groups pass in one fresh owned HVF guest. Together with the
53 accepted groups this completes **55/55** selected installed-service groups
and supports the fourth #67 criterion. This is source34 component acceptance,
not qualification of later PR327/328/329 manager/content sources or whole R1.

## Observed results

| Group | Result |
| --- | --- |
| D1 disabled installed service | Pass: expected CLI exit 1 and `optional_bus_support_disabled`; no readiness, owner/provider/window identity unchanged. |
| D2 ordinary installed browser without bus | Pass: actual state observed, actual Form Escape retires its window, owned browser wait 0. |

All fixture cleanup failure lists are empty: no unexpected X errors, retired
windows, live descendants or native close failures. Xvfb, private bus, core and
three module children wait 0. The prior eight enabled groups are retained through
exact actor/receipt identities and **were not rerun**. No transfer, Go/race,
theme, default-X or full support suite repeated. No new UI/screenshots.

The source pre/staged/post/postguard closure is
`7708bcbf4f45e31bb93cb9f20a140f30922be5e244ff9595699cfdfdb491d6ec`;
source34 archive SHA-256 is
`9726801aa8963c300e9cc6076289500e69a204511ba17e50b6503ee455c9550d`.
The unchanged fixture hash is
`c0dbf3745333b34bfedb480259afb5a74bce650a86d9c951a14a280d95e8d4c9`.
Five enabled built/installed actor tuples still match exactly. Disabled browser
SHA-256 is `51cf9f8b74f7fce9d4d16d08bbaf49d7d2c7ada0e270911395a31430c62c705a`;
disabled service helper is
`0a9c50cb60a9adbeee1a995a1b3783522249e3ca3eade6a842bf65ae50012847`.

## Inspected plan and profile

OpenBSD 8.0 ARM64, existing HVF/virtio configuration, 4 GiB/two CPUs and original
functional deadlines. OpenBSD Clang 22.1.6, fixed SDK/cache identities. The
dispatcher verifies 25 pinned inputs and the exact archive before launch.
The reviewed plan changes only prefix and enabled-to-disabled session bus;
settings portal stays false. Effective `licensedir == ''` is preserved by
omitting that explicit setup argument. `depmf.json` stays absent; selected
install destinations rebase exactly. No `''`/`'.'` equivalence is asserted.

Plan SHA-256:
`8f18fb6be8f03e4854e257c88d01ee8d62c7af8b53499e5ef51be262186ee485`.
Inputs SHA-256:
`7600884943b587f78ad3e1f0b026ed800c081f929c44ca59cf0eb2837f8ba851`.
The 266,947-byte inspected and executed plans match. The 111 admitted build
commands cover four selected executable leaves, their library prerequisites
and two manuals; no whole WM/module build or suite. Shared Go cache use remains
owned and nonconcurrent. Selected tags install only browser/session artifacts.

## Evidence, failures and cleanup

The exported archive is 225,870,149 bytes, SHA-256
`517b86740eaeef3d926fb34b636eef095448d529ff22c24a1f5836d515c867ef`.
The guest packaging manifest's **98/98** receipt files match exported bytes.
Lead and separate workflow inspection verify the package, actual two groups,
source/actor guards and retained enabled receipt. Compact current receipts are
retained separately under the private shared-checkpoint workspace.

Private dispatch:
`builddir/codex-2026-10-31/r1-script-selection-checkpoint/q67-openbsd-arm/arm64-owned-run02`.
Compact verified receipts/proof:
`builddir/codex-2026-11-01/r1-content-shared-checkpoint/q67-receipts` and
`q67-retained-proof.json`. Raw historical tray/cache results in the export are
not fresh passes. Retained paths and later retirement are recorded separately.

The earlier empty-directory setup failure and source-copy/network-reset attempt
remain historical failures. This successful unchanged-profile attempt does not
diagnose their cause. QEMU PID13925 is confirmed absent; runner exit 0 follows
its SIGTERM cleanup, **not normal guest poweroff**. The unrelated NetBSD guest
is untouched. After verifying export and absence of owned handles/process, the exact stopped
clone and duplicate inputs were retired: 5,879,726,080 allocated bytes removed,
not a promised APFS free-space change. The current archive and 98 verified
compact receipts, reusable base assets and SDK/build caches remain. The private
`q67-retirement.json` records exact retired paths and retained proof.

Reconcile #67 to 4/4 on reviewed integration of this report. Keep #22's new
manager runtime, #63/#71/#74 protected content, Script's Fontconfig clean-exit,
native hardware and release-wide qualification separate. No performance-counter
increment or benchmark is attached to this verification-only closure.
