# CI-01 fixed asset preflight — 2026-10-13

Status: affected local checkpoint qualified. Provider execution integration,
candidate default-X, actual native CI and workflow adoption remain open.
No WM runtime, configuration contract, installed utility or dependency changes.

## Scope and contract

Three related stories add strict selection of the existing thirteen release
pins, a verified private asset/cache owner, and bounded acquisition/publication
controls. [ci_assets.py](../../tools/maintenance/ci_assets.py) uses Python stdlib
and the existing `common.fresh_output` helper. It checks exact release URLs,
publisher/local hash agreement, sizes and complete profile membership. It refuses
unverified, duplicate, missing or inconsistent lock entries.

Every cache hit is rehashed from the same opened descriptor copied to the new
output. Cache files must be regular, owned, private and unlinked to other names;
symlinks, FIFOs and directories are refused without waiting for a writer. Cache
and consumer files use separate inodes and mode 0600. The cache directory is
mode 0700. Source-owned pins, developer parent paths and workspace are trusted;
this is not a privileged service or hostile-workspace sandbox.

Fresh downloads run in a subprocess with a configurable total deadline of
1–300 seconds, default 300. The parent terminates and reaps a stalled worker;
urllib's socket timeout alone is not treated as a total deadline. HTTPS downgrade
redirects are refused before the redirected request. Size and SHA256 must match
before cache publication. A complete private output directory publishes once;
existing output, existing corrupt cache content and prior evidence are preserved
on refusal. No downloaded program, launch script or image is executed, extracted
or booted. Local disk reads have size bounds, not a separate wall-clock deadline.

## Source and evidence

The final four-file source archive is
`4bd0566feb58b853594d086ddb14336bc513755d4de85857459607f5a0e66a2a`.
It contains the new tool/test, unchanged shared publication helper and unchanged
asset lock. The initial archive is
`70de21d6030cb79282dc2dc405202696280a8650e4b04d6a8abc0376bc458881`;
the only final change extends ordinary acquisition fixture startup from one to
ten seconds. Production tool/common/lock bytes are identical. The independent
one-second stalled-worker control is unchanged.

- [Source manifest](receipts/ci-assets-2026-10-13/source.json)
- [Acceptance record](receipts/ci-assets-2026-10-13/acceptance.json)
- [Validator](receipts/ci-assets-2026-10-13/validate.py.txt)
- [Fresh download manifest](receipts/ci-assets-2026-10-13/downloaded-manifest.json)
- [Offline cache manifest](receipts/ci-assets-2026-10-13/offline-manifest.json)

Six target archives preserve the exact source, platform/Python metadata, test
log and CLI help. The validator checks source membership/hashes/modes, every
passing row and source identity, plus actual output size/hash/mode and independent
cache/output inodes. Meson registers `ci-asset-contract` with a 30-second budget;
this batch runs the test directly on each target rather than claiming another
whole-project configure/build/install pass.

## Results and retained failures

| Check | Result |
| --- | --- |
| Linux AMD64/ARM64, FreeBSD AMD64/ARM64, OpenBSD AMD64/ARM64 | 23 tests each; 138 native guest test executions |
| macOS development check | 23 tests pass; supplementary host evidence |
| Actual fresh release downloads | All thirteen verify; 3,832,706,906 bytes total |
| Actual offline reuse | All thirteen reverify; independent private output files |
| Worker failure/ownership controls | Corrupt/short/oversized content, missing offline entries, unsafe cache types/permissions, concurrent publication, partial output cleanup and total-deadline refusal pass |

The five existing SSH test guests were stopped, so verification boots independent
child overlays from the retained qualified disks. The first collector used
`tar xf` for a gzip archive; both OpenBSD runs stopped before tests. Explicit
`tar xzf` corrects the collector. OpenBSD AMD64 then reached four ordinary worker
startup timeouts under the fixture's one-second deadline; its original test log
and guest source remain. The final fixture allows ten seconds for ordinary
success/refusal workers; the separate intentional stall still times out after one
second. No production deadline was widened and no failed assertion was retried
away. All six final test logs pass. Linux ARM64's first non-terminal Podman startup
reported success but its VM was stopped before transport; a diagnostic terminal
startup remained available through the passing container check. The underlying
startup cause is not established. Existing VM/container configuration is unchanged.

The scoped diff check passes. A whole-workspace diff check reports whitespace in
separately owned generated Script parser files; this batch does not modify them.
The accepted utility/core evidence is retained. No X-visible or C change warrants
repeating the full compiler/default-X/sanitizer matrix for this tooling checkpoint.

## Remaining adoption gates

The pinned upstream action always downloads AnyVM itself, without checking the
recorded digest. Its extracted/prepared image caches are mutable and lack fixed
content validation. A preflight step that verifies a different copy cannot protect
those execution paths. This helper therefore is not wired into a candidate as
false execution enforcement; upstream provider integration remains CI-01 work.

Fixed SSH/firmware inputs, expanded-image/cache identity and provider framebuffer/
input/default-X need their owning review and actual boot tests. The OpenBSD AMD64
plain provider's virtio VGA is distinct from the accepted local vesa setup.
Candidates stay inactive. The checkout still has no Git remote, so actual Actions
execution is unavailable; local QEMU/container results are not native Linux
release ASan/UBSan CI, physical hardware checks or a public release certification.

The linked receipts are exact copies of the original local checkpoint data.
The validator is archived as text, without adding an active runner or changing
source/acceptance identities. Remaining raw logs stay in the local evidence tree.
