# Portable browser launch requests — 2026-10-23

Status: **focused checks passed; current feature integration pending**.
Owners: FILE-DATA-01 / FILE-ACTION-01 / portable FILE-INTEGRATION-01 subset.
Base: merged PR166, `b95de9e521b5f290bebef04a93e3bb8a4c5acd3e`.
[Receipt](VERIFICATION-2026-10-23-FILE-BROWSER-HANDOFF.json) ·
[Contract](../decisions/FILE-BROWSER-HANDOFF-2026-10-23.md).

## Delivered batch

1. Literal absolute paths and a bounded local file-URI subset retain original
   filename bytes, including invalid UTF-8, with exactly one percent-decoding
   step. Refuse remote authorities, user info, ports, opaque/relative forms,
   malformed escapes, NUL and query/fragment syntax. Ordinary absolute paths
   retain literal percent/hash/question-mark characters.
2. --reveal admits one to 256 entries in one lexical parent. The existing
   cancellable worker loads and resolves all requested names before publishing
   a snapshot and stable IDs. Missing/duplicate/mixed-parent/root/dot-component
   requests fail without partial selection. Requested dot entries enable hidden
   entries; the first target's page is shown. No target is opened or executed.
3. --properties shows one requested entry's observed metadata immediately,
   including symlinks/special entries without following or activating them.
   --inspect supports the same modes and returns selected_ids in request order.
4. A generation-checked complete adapter selection validates known IDs and the
   existing cap atomically, independently of paging. Native page callbacks keep
   their narrower policy. Worker, wire, queue and cancellation bounds remain.

Installed syntax/examples and the architecture/tracker contract are updated.
No C/header, native packet, install/default binding or database dependency
changes. The full virtual widget/general provider/browser handoff, native
watching, mutations and FileManager1/default roles remain open.

## Verification and source binding

The final frozen 2241-file archive guards ten changed runtime/manual/fixture
inputs. Linux ARM64 uses patched Go 1.27.2 and the existing incremental optional
Go profile. Its registered file-browser-policy entry passes: five packages/33
top cases. JSON policy evidence and the scoped race run also pass those five
packages/33 cases; vet passes. Test executions repeat the same inventory and
are not counted as distinct cases. The unrelated standalone command-client
Python entry is not repeated at this final feature gate.

Native FreeBSD 15.1 ARM64 passes the same final five packages/33 cases, vet,
build and the actual binary's --inspect --reveal request. Its complete Go source
hash/mode manifest matches the frozen candidate. An earlier native development
pass exercises the filesystem work before final URI-authority/probe refinements;
the final receipt qualifies the corrected full inputs.

The final Linux actual-X driver passes existing file actions, stale controls,
blocked worker cancellation/reaping and new target requests in three browser
instances. It verifies hidden selection across pages, initial symlink properties,
non-UTF-8 URI target inspection, remote/missing request refusal and an independent
opener observer showing no implicit activation. Nine native screenshots are
captured/hash-verified; the two new reveal/properties views are visually inspected.
The unchanged core/MFL/Form binaries retain the qualified prior-cycle identities.

Local make verify passes 177 portable cases, with 14 explicit unavailable native
tool skips. Final repository/diff/format review passes. The updated generated
manual renders with native FreeBSD mandoc and checks the new flag/result fields.
Lint status 3 records Asciidoctor preamble mso/LINKSTYLE errors, source-line style
notes and section-spacing diagnostics; clean lint is not claimed. No packaging
paths/tags change and no new full staged-install qualification is claimed.

The initial development passes are preserved. Local review tightens ASCII
localhost matching to refuse a Unicode lookalike and strengthens the native
opener observer; final checks use those corrected inputs. A README word-limit
failure is corrected before freeze. No final selected gate fails or needs a
post-freeze runtime/fixture correction. Reports/status-only upkeep afterward
leaves the ten qualified inputs unchanged.

## Integration, storage and remaining gates

Before implementation begins, the accumulated PR164–166 checkpoint closes on
all six Linux/FreeBSD/OpenBSD 8.0 AMD64/ARM64 targets: 63 selected core/desktop
entries on seven affected profiles, five Go packages/26 cases per target, six
native browser drivers and exact retained Linux ARM64/race/scoped sanitizer
results. See the separate [cycle report](VERIFICATION-2026-10-23-FILE-SUPPORT-CYCLE.md).
That qualification is for the merged reference, not these new 33-case features.
This first new candidate uses the focused cadence, cycle 0/3 merged, 1/3 on merge.
Further accumulated six-target and full compiler/install/default-X/owner/release
coverage remain at their boundaries; native-CPU/hardware/toolkit/compositor/
assistive coverage remains separate. PR164 performance reference: 2/5 merged,
3/5 on merge; no comparison is due and tuning remains deferred.

SQLite is deferred. File browsing owns live filesystem observations; current
collection IDs/generations and future native content handles are not database
keys or table schemas. No SQL-backed history, generic storage abstraction or
clipboard payload is introduced. STORE-01 still owns protected memory first,
then optional versioned encrypted persistence/recovery and backend adoption.
Clipboard secrets do not enter these ordinary file/Form/Echo metadata paths.

Canonical evidence is under the workspace root's
`builddir/codex-2026-10-23/file-browser-handoff/`, including the separate cycle,
Linux final archive, final FreeBSD archive, source manifests and native screenshots.
Complete archive-member hashes/modes and gzip CRC are verified. A private signed
publication record binds final commit/tree after documentation upkeep.
