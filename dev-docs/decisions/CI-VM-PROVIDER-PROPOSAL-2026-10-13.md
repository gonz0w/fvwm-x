# CI-01 VM provider choice — 2026-10-13

Status: approved by the user on 2026-10-13; implementation and provider
runner qualification complete; fresh dependency/build/install acceptance
remains gated by the separate patched PNG delivery choice.

## Concrete reason

The selected vmactions actions download AnyVM immediately before execution,
without enforcing the project's recorded digest. AnyVM can boot mutable extracted
or prepared cached images. The qualified
[asset preflight](../verification/VERIFICATION-2026-10-13-CI-ASSETS.md) protects
its own downloaded/cache/output bytes; an earlier preflight step cannot protect
the action's separate runtime download or those mutable boot inputs.

No candidate has been activated. There is no supported action input for a
preverified runtime. Continuing with a different provider changes an external
dependency, so it is not inferred from a code cleanup. [AGENTS.md](../../AGENTS.md)
says: “Do not implement a broad rewrite, API rename or dependency replacement by
inference.” The prior choice to use local QEMU remains in force; this decision
concerns which wrapper owns future CI VM acquisition and execution.

## Approved option: repository-owned QEMU gate

Replace the inactive BSD candidates' vmactions/AnyVM wrapper with a small
repository-owned runner using existing QEMU, SSH and zstd tools. Keep the
reviewed fixed builder image/profile inputs, six OS/architecture targets,
separate Linux sanitizer/Go profiles and existing native/default-X test drivers.
No new WM library, GUI toolkit or desktop package is implied. This is CI tooling;
FVWM configuration files and installed runtime programs are unchanged.

The bounded implementation would:

1. Consume the verified compressed image/profile from CI-01's asset store;
   independently verify any additional SSH/firmware inputs before use.
2. Derive each boot image from those verified compressed bytes in a fresh job
   directory. Reuse only fixed verified compressed cache inputs initially; no
   prepared or unsigned expanded-image cache. Bound extraction and boot ownership.
3. Use explicit per-profile QEMU arguments, isolated disks/firmware, loopback
   forwarding and private job authentication. Launch commands are constructed
   from reviewed fields, not executed from a downloaded `.qemu` script.
4. Copy the reviewed source scope, run existing portable gates, collect complete
   logs/identities and terminate only the runner's own processes. Preserve failed
   acquisition, startup and test evidence.
5. Qualify all four actual provider images and their default X servers before
   adopting BSD workflows. Resolve OpenBSD AMD64 virtio VGA versus the currently
   qualified vesa profile through actual driver/input checks.

The benefit is one explicit acquisition/boot owner with no downloaded Python VM
manager or provider action cache lifecycle. The cost is maintaining a narrow
four-profile QEMU/SSH runner and doing fresh provider-image qualification. This
does not authorize a general VM framework, arbitrary image/profile inputs,
publishing, upstream notifications or a whole-CI rewrite.

## Alternative: retain the upstream provider

Keep the candidates inactive until the upstream provider supplies fixed digest
enforcement for runtime/image/profile/SSH/firmware inputs and every cache hit,
with an execution contract we can qualify. Local QEMU verification and other
tracker work can continue. Contacting upstream would require separate explicit
authorization; none has been sent.

## Acceptance and limits

For either option, prove changed-byte and stale-cache refusal before execution,
finite cleanup after download/extraction/boot/SSH failure, actual installed
consumer identities, all four provider default-X profiles and truthful artifact
provenance. The accepted local asset and utility checkpoints are retained.

Actual GitHub Actions execution still needs a repository/remote and selected
runner access. QEMU on macOS is local evidence, not native Linux release sanitizer
CI or physical GPU/input qualification. Publication remains separately owned.


## Local implementation checkpoint

The affected runner scope is qualified locally on six targets, with actual
pinned-image boot/owned authentication, failure/cancellation controls and all
four authenticated default-server startup profiles. OpenBSD AMD64's optional
VESA test setup is explicit and confined to its disposable guest; ordinary jobs
retain the image's aperture setting. No additional WM dependency or configuration
migration was introduced. The candidate remains inactive pending fresh patched
package/build/install/default-X gates and actual remote execution.

[Runner evidence](../verification/VERIFICATION-2026-10-13-QEMU-PROVIDER.md).
[Separate external input proposal](CI-PNG-INPUT-PROPOSAL-2026-10-13.md).
