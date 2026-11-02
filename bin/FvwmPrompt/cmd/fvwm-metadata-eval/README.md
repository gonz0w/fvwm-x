# Public metadata backend experiment

This manually selected development command compares the actual bounded Go
filemodel/collection pipeline, an evaluation-only JSON snapshot, and optional
SQLite/FTS5. It is excluded from normal builds and is not installed. It does not
scan a directory, read file contents, change the browser or index clipboard data.
[Scope, acceptance and execution queue](../../../../dev-docs/decisions/DIRECTORY-BACKEND-EVALUATION-2026-11-02.md).

Use the approved SDK in [INSTALL.md](../../../../INSTALL.md). Work from
`bin/FvwmPrompt`, with `GOTOOLCHAIN=local`, `GOPROXY=off`, `GOSUMDB=off` and
`GOFLAGS=-mod=vendor`. Put outputs and GOCACHE in the owner's evidence/build
area. No module/toolchain download or system package change belongs to build.

Build the baseline with CGO disabled:

```sh
CGO_ENABLED=0 go build -tags=fvwm_metadata_eval \
    -o /absolute/owned-area/metadata-baseline ./cmd/fvwm-metadata-eval
```

The explicitly optional variant needs matching target SQLite C headers/library
and a C compiler; there is no Go driver dependency:

```sh
CGO_ENABLED=1 go build -tags=fvwm_metadata_eval,fvwm_sqlite_eval \
    -o /absolute/owned-area/metadata-sqlite ./cmd/fvwm-metadata-eval
```

Before comparison, freeze source, SDK/compiler, headers, library/build/runtime
identity and features for each target. `--describe` emits the binary's own
capability/header/runtime identity without constructing a corpus/database.
A SQLite run requires `--expect-sqlite-source` with that exact frozen runtime
source ID. An absent optional build/module is unavailable, not a passing result.

At the single named evaluation checkpoint:

- Compile and execute tests with the same selected tags and native SDK. The
  hand-authored small oracle covers four sort keys/directions, literal filter,
  duplicate labels, updates, stale generations and bounded complete readback.
- For each backend, run `--run --mode=verify --count=12 --repetitions=3`, then
  the frozen 0/256/4096 corpora. SQLite also needs the exact source-ID argument.
  This mode includes reference work and is correctness evidence, not backend RSS.
- Run bounded repeated `--mode=measure` invocations with identical inputs,
  repetitions and binary identities. Require every `result_sha256` to match its
  verified reference and cross-backend result. Measurement mode omits reference
  queries and fault controls; its status explicitly requires that comparison.
- Run `--run --mode=controls --count=12` separately for snapshot and SQLite.
  Controls use their own private subdirectory, so they cannot alter workload
  file sizes or contaminate measurement high-water RSS.
- Record full process startup, raw OS memory units, loaded/build/install closure
  and repeated variability outside the command as required by #66's rubric.

The literal workload uses actual Go normalization and preserves raw names as
bytes. FTS uses a separate six-prefix ASCII token experiment; it does not change
browser search semantics. FTS index construction and repeated token queries are
separate phases, with a timed scan baseline. Storage before FTS and its incremental
file bytes are separate. Final file bytes are not peak journal/storage costs.

Every invocation owns a fresh 0700 temporary directory and removes only that
tree. Snapshot publication tests rename/process reopen, SQLite uses its recorded
transaction/durability settings; neither proves power-loss recovery. The bounded
full case limits SQLite pages, not free disk space, and checks rollback after
partial replacement. No background service or idle-work claim follows.

The source checkpoint's compile receipt is not an executed oracle, comparison,
adoption or six-target pass. See the current tracker before executing queued work.
