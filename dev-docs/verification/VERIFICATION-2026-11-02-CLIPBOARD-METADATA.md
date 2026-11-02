# C2 clipboard metadata handoff: source and build checkpoint

Four related stories provide versioned native-owner capabilities, bounded
metadata pages/actions, the installed `fvwm-clipboard` client/manual and a
configured-session lifecycle fixture. Read-only frontend cancellation no longer
retires the session history owner; canceled mutations/barriers retain their
fail-closed behavior. No clipboard payload enters Go or public JSON.

The owner selected configurable text defaults: 32 items, 1 MiB encoded payload,
32 KiB per item and a 10-minute upper-bound retention window. Checked page/guard
charges remain separate; these budgets do not promise OS locking or enable the
service. See the [canonical decision](../decisions/CLIPBOARD-PRIMITIVES-2026-10-06.md),
[manual](../../doc/fvwm-clipboard.adoc) and [source-bound build record](VERIFICATION-2026-11-02-CLIPBOARD-METADATA.json).

## Observed

- Runtime source derives from merged PR343 `8637cc7ad89701ed4c084d469869245eae58ae1e`.
  Integration base is PR344 `4d43383ed1a414c5ab912a03c79ec6ce652f82fe`; its Temper
  documentation-only refresh is preserved. All affected code/build inputs and
  compiled artifacts remain unchanged through this rebase.
- Five final affected host compile/link commands pass with the approved offline
  Go 1.27.2 SDK and existing vendored dependencies/cache: clipboard,
  sessionservice, sessionowner and CLI test binaries, plus the installed CLI
  executable. No test binary or CLI example was invoked.
- Separate source review resolves the unobserved-status/false-exit report and
  final-page lock-fence coverage gaps. The lifetime correction drains one
  correlated abandoned read reply under its existing bound, permits no new wire
  request while it is active, and rejects submissions after Stop is requested.
- Repository, registration and whitespace/source checks pass. The primary man
  page is registered under `clipboard-client`. Host Asciidoctor is unavailable;
  rendering and installation remain queued.

## Authored acceptance, not execution

New cases cover page bounds/progress, exact refs, malformed operation replies,
lock changes during rows and the final status fence, canceled/busy observations,
page/guard arithmetic and default policy, CLI syntax/correlation/JSON/refusal,
entered-read cancellation and Stop-before-Wait rejection.

The configured Run fixture uses an independent synthetic numeric peer and
harmless existing WM/discovery/locker subprocesses. It asserts UNKNOWN startup,
one owner across core exec, pause acknowledgement before locker construction,
failed pause preventing Start, locker exit never unlocking, logout EOF and
exact confirmed cleanup reporting. This does not test native OS protection or
secure locker readiness. These assertions have been compiled, not run.

Runtime joins the named C2–C5 clipboard consumer checkpoint under the approved
four-to-six-PR cadence. There were **zero runtime invocations and zero lab
starts** for C2. No red/green runtime history is claimed for source-discovered
corrections. Reuse C1's evidence only for its unchanged native C component;
changed Go actors/clients and new orchestration still require their queued runs.

## Remaining boundaries

Installed capture stays off pending qualified lock-state integration. The CLI
cannot set STATE, stop the owner, enable capture or configure history. It is a
metadata consumer handoff, not the complete text/image manager. No payload
search/display/restore/export, image decoder, persistence/crypto, new mandatory
package or drawn UI is adopted. No new screenshot/theme run is applicable.

Pages are observations rather than payload leases or a consistent snapshot;
exact action refs still revalidate. Blocked pages contain no descriptors.
Capability preflight facts and configured budgets are distinct from protection
of a future allocation. A failed observation does not imply observed exit.

Full Meson/compiler/feature/platform/install/default-X/hardware/physical-lock
and wider owner-issue acceptance stay open. No issue or milestone closes from
this source-only checkpoint. Performance is 1/5 before delivery; advance on
integration without a redundant benchmark.
