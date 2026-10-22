# Contributing to FVWM-X

Read [AGENTS.md](AGENTS.md), the [tracker](dev-docs/PROJECT-TRACKER.md), and the
[developer guide](dev-docs/DEVELOPERS.md). The tracker owns execution order and
scope; GitHub issues and the Project mirror that order and track review.

Use a bounded `codex/` branch from `origin/main`. Batch related implementation,
tests and documentation into one reviewable PR per coherent checkpoint, usually
three to five substantive related behavior improvements. Tests, probes and
documentation supporting one improvement count as that one item. Use focused
incremental checks between items. Ordinary PRs use focused merge gates; share
one affected platform checkpoint across roughly three feature PRs and complete
full acceptance at milestone/release boundaries. See the
[cadence](dev-docs/WORKFLOW.md#implementation-batches-and-test-cadence).
Keep related docs/evidence and review fixes in
that PR while it is open; split for independent contracts or ownership. Preserve
another chat's edits and run from an isolated, identified candidate. Sign contributor commits and
include the author DCO trailer with `git commit -S -s`; signatures and sign-offs
serve different purposes. Configure a trusted signing key per clone.

```sh
make verify
# Explicit opt-in; hooks change this clone's Git configuration only:
make hooks
```

`make verify` runs local repository hygiene and portable maintenance/gate policy
checks. It does not compile the WM or establish native platform, X11, race,
sanitizer, hardware or release acceptance.

Run it once per shared integration cycle, or when its harness changes, not after
each story or documentation edit. During development, use affected tests and an owner-controlled incremental
build per compiler/feature profile. Docs/status-only updates use `make check`
and whitespace/link checks; retain unchanged source-bound runtime evidence.
The offline link check covers current inline-Markdown file references; archived
plans, remote URLs, anchors and HTML keep their separate review scopes.
Add the affected native checks:

```sh
sh tests/run_on_linux.sh builddir/review-linux
sh tests/run_on_bsd.sh builddir/review-bsd
sh tests/run_native_gate.sh --go-clients builddir/review-go
sh tests/run_native_gate.sh --sanitizers builddir/review-sanitizers
```

Run native commands on the matching Linux/BSD environment. macOS may use the
qualified local QEMU/lab environments. Follow [support](dev-docs/SUPPORT.md),
[BSD setup](dev-docs/BSD.md) and [local recipes](dev-docs/ci/README.md), including
the patched PNG requirement. Normal PRs use affected checks on one incremental
Linux profile, plus early affected-platform checks for sensitive changes.
The full scripts above are acceptance recipes, not an every-PR checklist.
Record the integration cycle and deferred gates; reuse valid unaffected evidence.
Use separate build directories. Default-X tests require a disposable BSD VM.

Before pushing, inspect the diff, run appropriate checks and record commit/tree,
base, commands/results, source-bound native receipts and remaining gaps in the
PR. Never replace unrun checks with a success status. Local hooks are bypassable;
GitHub does not enforce local test evidence. Actions and status publishers are
not used. Leave PRs open for Cam (@gonz0w) to review and merge deliberately.

Use `Refs gonz0w/fvwm-x#N` for broader or partial outcomes; use `Closes` only when
merging satisfies every criterion of that specific issue. Keep one status and
priority label, the milestone and Project fields consistent. A component merge
does not close a broader release gate. Refresh the brief README status when a
milestone or product scope changes, and update the tracker/evidence together.
