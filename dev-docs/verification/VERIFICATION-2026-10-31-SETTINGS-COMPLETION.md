# Settings and preview completion — evidence in progress

CFG-01 #60 / SETTINGS-01 #73. **Acceptance remains pending.** This is a
working evidence summary, not issue closure or release qualification. The
frozen contracts are [preview completion](../decisions/SETTINGS-PREVIEW-COMPLETION-2026-10-31.md)
and [Settings portal](../decisions/SETTINGS-PORTAL-2026-10-31.md). Results below
are scoped observations; they do not establish all six target profiles or native
CPU qualification.

## Criterion map

| Criterion | Evidence and remaining work |
| --- | --- |
| Owned complete preview, recovery and root lifetime | Gate 21 passed its controlled normal recovery and existing fixtures. Gate 22 passed controlled 128-event real-child configure, complete native-X and owned-root shutdown checks; classic-large failed as described below. Gate 25 passed the corrected Slate, Dune and classic-large workflows in 88.84 seconds, plus UI policy and Desktop build. Combined acceptance remains pending. |
| Typed plans, native and Go policy | Gate 21 passed preview, MFL request, image background and PNG setup policy tests. Gate 15 passed affected Go race and vet checks and command metadata. Gate 28 passed full v3 in software PNG/no-Render and minimal no-PNG/no-SVG/no-Render profiles (22.984 and 20.883 seconds). Gate 30 passed actual legacy v1/v2 (23.651 seconds); its single worst 26-row test observed at most 8170 of 8192 reply bytes across eight logged cases. |
| Portal protocol and CFG source | Gate 04 passed frontend and CFG-to-frontend checks. The frontend profile exercised ReadAll, ReadOne, legacy Read nesting, signals, activation, restarts and CFG update. Native application cases driven by raw standard input do not themselves establish CFG provenance. |
| Application effects | Gate 14 passed the actual Firefox 159 profile with portal enabled and disabled. CSS reduced motion changed false → true → false with events; fresh and restarted clients observed true in the reduced-motion state. No portal-route attribution follows because both profiles behaved similarly. CSS fonts and device pixel ratio stayed unchanged. The Qt 5.15.13/PyQt 5.15.10 XCB default-Fusion profile observed font 9, hicolor and animation hints unchanged during updates; this is a profile observation, not a general Qt compliance judgment. Protected browser maps were unavailable as metadata. |
| Sanitizers and fault/topology | Sanitizer 01 passed nine older-source C unit checks and direct portal exercise. Gate 29 passed all nine leak-enabled sanitizer units and sanitizer core v3 (21.142 seconds); no sanitizer MFL or worker execution is claimed. Its allocation-fault run passed (19.011 seconds) with one actual wrapper fault witness. Xorg topology passed (20.757 seconds), observing 1920 → 1024 → 1920. |
| Install, support and manuals | Linux ARM gate 27 passed enabled/disabled builds and installs, six C policies in each, Go, catalog, portal/native wire, both Xvfb profiles and default Xorg. Its aggregate failed because guest mandoc was missing. Four generated/installed manuals were exported byte-bound and host mandoc UTF-8 rendering passed; lint did not pass: Desktop 3, MFL 4, Backer 3, portal 3. Thus the ARM contract results are composed evidence, not a passing aggregate. Other platform and final manual reconciliation remain pending. |

The Firefox profile used an official ARM64 executable, headful under uid 65534
with sandbox retained. Its x86_64 user-agent privacy string is not architecture
evidence. Private extracted packages were used without system installation or
new product toolkit dependencies. GTK 3.24.41 native-key, notification, fresh
and enlarged observations remain the qualified merged PR316 evidence. Fresh
Xcursor lookup was covered there; cached application cursor replacement is not
guaranteed.

## Failures and pending corrections

- Gate 22's classic-large editor measured 1508 pixels wide on a 1440-pixel
  screen and failed the control geometry assertion. A two-file Go layout
  correction was followed by gate 25's passing three-theme workflow; retain
  the original failure. Visual inspection of all nine gate 25 captures found
  underlying editor text visible through all three chooser menus, with
  overlapping text difficult to read, especially in classic-large. The small
  probe window's title is truncated in Slate and Dune. Visual inspection is not
  functional or accessibility qualification.
- Gate 23 passed software configure/build and background/preview policy tests,
  then failed a driver assertion based on obsolete Cairo-disabled expectations.
  This is a driver assertion failure, not a demonstrated product failure; gate 28 passed the corrected software and minimal profiles.
- Linux AMD64 platform 01 failed its disabled build because the inherited
  profile had `build-live-x-tests=false` and therefore lacked
  `tests/image-owner-probe`. Platform 02 passed native builds, installs and
  profile Xvfb but failed default Xorg at a one-second Wait setup race.
  Follow-up 03 failed earlier at a deleted-source one-second setup race. Analysis
  of source and logs supports lease expiry during pixel observations; this is
  an inference, not a passing run. Follow-up 04 using source20 passed the same strict default-Xorg checks in 219.14 seconds; unchanged installed production inputs are explicitly verified.
- FreeBSD ARM platform 02 failed before build because the old Meson 1.10 cached
  option registry did not recognize `settings-portal`. The helper now refreshes
  that registry without adding a new flag; platform 05 passed the full combined checkpoint in 129.86 seconds. FreeBSD AMD64 and OpenBSD ARM64 subsequently passed their complete combined checkpoints. OpenBSD AMD64 retains the preparation-observation failure and pending source24 rerun.
- Gate 10's overall actual-application run returned unavailable; its Qt profile
  observations are retained as such. Gate 14 supplies the later browser profile,
  not retroactive success for gate 10.

## Source and artifact identities

Working records are under `builddir/codex-2026-10-31/r1-settings-completion`
in the workspace. `source16.json` records base
`6630bb9edc54dd305e6914ca907bcc78ae4b5f03` and source closure
`28c76048098c3265fed4512fac6869cb5a95ba954af4e9681fa2c10b411020c2`.
This identity is not a claim that subsequent corrections have the same bytes.
Source18 closure `adf596283565573a3e5fd232188cd78936288ea1f9f730c32145cfce27525759`
binds the later production C/Go; source19 and source20 change only fixture timing
and captured entry replies, with production C/Go identical. Their closures are
`d0810bf1b79c5f53cb5352ee18a51a2abfa2e8f76905d78e28281438534c8c46` and
`bd8d4ffe3f30407494a5cbf76c7420d47cfc1969bbbf38cf44d91d7027058de3`.
Selected production SHA-256 entries in source20 (the UI row includes the layout correction):

| Source | SHA-256 |
| --- | --- |
| [appearance_preview.c](../../fvwm/appearance_preview.c) | `0eff24fa1f77dcc70ff33d858e68184d04bdbb4f65513e288b1494bc72441461` |
| [root_background.c](../../fvwm/root_background.c) | `f0913d0448c3e2ee8302e2ff8eaad099815a1de1568b41109289640aa538cd3d` |
| [preview_policy.c](../../fvwm/preview_policy.c) | `3803829cb106c1f8ca7906704d67fe344910956d6bc35c3243964ab6bae0170c` |
| [MFL request.c](../../modules/FvwmMFL/request.c) | `d44a3ae51ba65cb46a9b01118c1752c2bd3432abcb8f1799397a6c16e7c9b519` |
| [settings_portal.c](../../bin/settings_portal.c) | `5abf2cdbd2f21f9f644dd12364aaf940392018bb72cf0f982dbfbeb9fa47954b` |
| [desktop settings UI](../../bin/FvwmPrompt/internal/desktopsettings/ui.go) | `03c7931d2b2036ba3d36ff5e5a79c1471fa71e756e328546d2c49ab47038e880` |

Gate JSON records include script hashes and archive identities. Archive SHA-256:
gate 04 `f3629ebaf2a48579aad5fe05db36243a0be0c53095f7e01558ad14e23a4618cf`;
gate 14 `74579426fe70faec006aaa4c41fc7b5f1beb031ff1067d9159297e21b65b2a26`;
gate 15 `3ba115bcd0756befc1850ec5534bbc8bd09378d8d8dc2459ea084a90de784ff6`;
gate 21 `2b3ebf39753e5ea342527d1449a58c59527e8c5852ac1ad457d7fe25318abcb6`;
gate 22 `5c99f15fc43188e5f229b07b2e07fde333831363571c3ff1942cff7499b18f14`;
gate 23 `898e7a090c39eb9fbe7dfd01d578e0b3ab74ed00ddb7bfb357683ef82d851fc7`;
gate 25 `d21f4f58354e3e9ea38ab94835f4d37d29c834315b989838fc02d8b20b76bcf8`;
sanitizer 01 `55102e698482b9e81b5f114daf252f59f9620842e0972c7cc78cdaa1175f9d23`;
AMD64 platform 01 `a252facfafae0cb30f4e43f93af4a198ba46318f10eae3bc902df912e98c99b8`.

Host-verified later archive SHA-256: gate 27
`763beca6882d27ce5703686f5ba106caceba955d2f5d46f18c09a647e0675196`;
gate 28 `48209a24761f4e27b55bca60b188d78aae2e4489cff03910f24858e1767970bf`;
gate 29 `31d5ea64c91d2bc8bdf89a01fe7b2e1be747c0a69d1065d5626a953664bf2f36`;
gate 30 `b7ca2d7dca118dd7e6384b090fa1efe4e1b4ad1d2ae0149e1e92b6089ca66e12`;
AMD64 platform 02 `241b0347c08bd0cea67de92fd566fae38ce1f8acd74819d072b73b1745c7f52c`;
platform 03 `184b5f19af76aede1071483fdd8f2c0779f8539f2af96e79299b43d503b1477a`;
manual export `c72dd18413f39907a124b83f58af95c5151e7020b8dc4059c8e5a01f6680472c`.
Individual member identities are retained in `evidence/*-verified.json`; host
manual render outcomes and four byte hashes are in `manual-render-host.json`.

## Final qualification table

| Target/profile | Result and source boundary |
| --- | --- |
| Linux ARM64, native CPU/Apple Hypervisor | Source18 gate27 build/install, both nested-X profiles and default Xorg pass; guest formatter unavailable. Exact exported manuals render on host. |
| Linux AMD64, emulated CPU | Source17 platform02 builds/installs, policies, Go, portal/native wire and both nested-X profiles pass; source20 fixture-only follow-up04 default Xorg passes. |
| FreeBSD ARM64, native CPU/Apple Hypervisor | Source20 platform05 full combined enabled/disabled/default-X/manual rendering passes. Lint3/4/3/3 are nonpasses. |
| FreeBSD AMD64 | Source20 platform05 full combined enabled/disabled/default-X/manual rendering passes (1034.15 seconds). Lint3/4/3/3 are nonpasses. |
| OpenBSD8.0 AMD64 | Source20 build/install/policy/Go/portal/native-wire pass; v3 expiry setup failed as described below. Source24 fresh-guest follow-up is running. |
| OpenBSD8.0 ARM64 | Source20 full enabled/disabled/default-X/manual checkpoint passes, with5701 collected archive members independently audited. |
| Final chooser UI | Source21 changes only two portable Go files for a private opaque chooser; source22 restores the canonical generated Perl timestamp0. Gate 31 passed all three full theme workflows (89.097 seconds), two Go packages (4.09 seconds), Desktop build (2.08 seconds), and actual normal installation with byte-equal FvwmDesktop and canonical epoch-zero Perl metadata. Inspection of all nine new captures found opaque chooser rows without the gate 25 underlying-text bleed. The small fixture probe title remains truncated in Slate and Dune. Inspection does not establish accessibility or functional correctness. |

Gate 31 source22 closure is
`509c0317cab676cbdf318c86ada18c6ea2ba9202ab7f343b597ce198c4ff67cd`;
host-verified archive SHA-256 is
`67149ffdb1033cdd30b8c2deacead20274454ca39268f3ede247638a15a8755b`.
The [gallery capture manifest](../../doc/screenshots/settings-completion-20261031-manifest.json)
binds three unmodified images to their configurations. The original gate 25
visual failure and its archive remain recorded above.

Portable hygiene: the initial maintenance99-test run had one generated-metadata
failure and20 native-tool skips. Canonical epoch0 regeneration corrected it;
both affected metadata tests passed. The thirteen remaining verification scripts
passed193 tests. The composed maintenance result is272 passes/20 skips;
this is not a successful rerun of the original `make verify` invocation.
The original failure remains recorded, and source/code catalog parity is retained.

Retention:32 fully host-verified guest evidence trees/archives (152,967,548 bytes)
were retired. Nineteen superseded full source archives (523,362,044 bytes) were
replaced by3,534,465 bytes of verified changed-input deltas plus the source20
basis. All original manifests and payload closures remain reconstructible;
the original gzip streams are not retained. Current builds, useful installed
prefixes, SDK/dependency caches, private test runtimes and performance reference
remain. Linux ARM64/AMD64 and FreeBSD ARM64 labs stopped normally with observed
QEMU exits. FreeBSD AMD64 also stopped normally. Both first OpenBSD disposable runners exited zero through their established cleanup; this does not establish normal guest poweroff. The source24 AMD64 follow-up is running.

Pending lead reconciliation: record each required profile, exact final source
and artifact identity, commands, pass/fail/skip/unavailable outcome, retained
failure, manual review and separate review before any closure decision.

## Fresh snapshot fixture correction

OpenBSD AMD64's source20 first reverted case passed. The next expiry case
returned `preview_finished` at APPLY: preparation's fixed five-second deadline
was still running while the fixture launched a new observer for a fresh snapshot.
Debug timestamps bound that single snapshot at4.882616 seconds. This supports
preparation-deadline exhaustion during observation; the exact admission/expiry
instant was not recorded. No production deadline or recovery rule changed.

Source23 adds an optional non-installed persistent probe mode and reuses it only
for v3 fresh snapshots. It accepts one bounded serial request, issues the real
`Send_ConfigInfo`, resets at actual Colorset0 and requires ascending actual IDs
through native `M_END_CONFIG_INFO`. The separate passive publication observer
and every field/property/pixel/scalar assertion remain. Gate32 compiled but
failed because the new parser counted eight bytes for the nine-byte `Colorset `
prefix. Source24 corrects that literal boundary with `sizeof`.

Gate33's source24 probe build and actual-X v3 pass in31.145 seconds. It records
107 fresh snapshots (median54.485 ms, maximum60.594 ms), five observed helper
processes and core exit0, without X observer errors. All nineteen pre-APPLY
receipts were still prepared with at least4875 ms remaining. These are Linux
harness observations, not a WM performance comparison or an OpenBSD pass.
Allocation and topology are skipped in gate33 and retain gate29's actual passes.
The original reviewed fresh-guest OpenBSD driver is reused for the remaining
AMD64 qualification, avoiding new cache or credential-recovery machinery.

The two source24 test files have hashes
`bdfdf6db304844b4e9316963040cac5d79aec5357f713a8380c6cd9068bf1a21`
(probe) and
`3e98af7443f252bf7216c8e072f4f5e5f552eb289d63741281ddb8e36d64c58a`
(fixture). Source24 closure is
`5a5fbaf1f88194c49eaeddb9a8c57359141ce693a3d9f96af78344d725416205`.
Gate33 archive SHA-256 is
`ad6e9aa6ddf0e857dc351362c933e6ab6f11d17ac85fa9a58903f16440972ce5`.
OpenBSD ARM64's independently audited archive is209,043,611 bytes,
SHA-256 `3792b4b238fe646e8e654b7d1f24a2980bc82d284789726a0d94bc0e0555b924`.
OpenBSD AMD64's original failed archive is272,391,768 bytes,
SHA-256 `7b6e94bf16aedd1e416669a4f8d5149d92d5836a17846673cde1941811aa00e5`.

Final staged whitespace validation removed one redundant EOF blank line from
`libs/image_background.c`. Its C tokens are unchanged from qualified source24;
no native behavior or profile result is inferred from this formatting correction.
