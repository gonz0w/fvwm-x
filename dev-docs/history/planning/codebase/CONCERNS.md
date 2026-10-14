# Codebase Concerns

**Analysis Date:** 2026-08-29

## Tech Debt

**Large, tightly coupled core translation units:**
- Issue: Core behavior is concentrated in very large C files: `fvwm/menus.c` is 7,229 lines, `fvwm/move_resize.c` is 5,728 lines, `fvwm/style.c` is 5,604 lines, `fvwm/borders.c` is 5,331 lines, and `fvwm/events.c` is 4,802 lines. These files mix state transitions, X11 calls, parsing, drawing, and event-loop policy.
- Files: `fvwm/menus.c`, `fvwm/move_resize.c`, `fvwm/style.c`, `fvwm/borders.c`, `fvwm/events.c`
- Impact: A local change can affect focus, stacking, geometry, drawing, and event delivery. Review is difficult and isolated unit testing requires extracting substantial global context.
- Fix approach: Extract pure geometry, policy, parser, and state-transition functions behind narrow headers. Follow the existing extraction pattern in `fvwm/event_loop_policy.c` and `fvwm/module_runtime.c`, which can be tested without a running X server.

**Global mutable state and implicit lifecycle ordering:**
- Issue: Process-wide state is shared directly across modules through globals such as the display, screen, module pipes, parser arrays, event queues, and static lists. `fvwm/module_runtime.c` owns static live/death module lists; `fvwm/events.c` owns static event-loop timeout state; `modules/FvwmScript/FvwmScript.c` and `modules/FvwmScript/Instructions.c` expose many globals across translation units.
- Files: `fvwm/module_runtime.c`, `fvwm/events.c`, `fvwm/externs.h`, `modules/FvwmScript/FvwmScript.c`, `modules/FvwmScript/Instructions.c`, `modules/FvwmScript/types.h`
- Impact: Reentrancy, teardown, restart, and nested command execution depend on call order rather than type-enforced ownership. The death-row reference scheme in `fvwm/module_runtime.c` and recursive queue draining in `fvwm/module_interface.c` are especially sensitive to missed cleanup.
- Fix approach: Introduce explicit runtime/context objects and pass them to extracted subsystems. Keep ownership and cleanup in one module, and preserve the bounded/reentrant policies already isolated in `fvwm/event_loop_policy.c`.

**Duplicated and unsafe string handling:**
- Issue: The repository still contains direct `strcpy`, `strcat`, and `sprintf` calls in core and module code. The project TODO explicitly calls for auditing them, while multiple ad hoc builders remain.
- Files: `dev-docs/TODO.md`, `fvwm/expand.c`, `fvwm/windowlist.c`, `modules/FvwmIdent/FvwmIdent.c`, `modules/FvwmScript/Instructions.c`, `libs/Module.c`
- Impact: Every caller must independently prove destination capacity and termination. Several concrete overruns are described below.
- Fix approach: Use `xasprintf`, `strlcpy`, `strlcat`, or a growable string abstraction from `libs/safemalloc.c`; require checked lengths at the boundary where module packets, config text, and X properties enter the process.

**FvwmScript uses fixed arrays and unchecked parser stacks:**
- Issue: Script objects, IDs, nested blocks, argument layers, per-layer arguments, child scripts, and script arguments have unrelated hard-coded limits. `AddLevelBufArg`, `AddBufArg`, and `EmpilerBloc` do not check their fixed array bounds.
- Files: `modules/FvwmScript/script.y`, `modules/FvwmScript/script.c`, `modules/FvwmScript/types.h`, `modules/FvwmScript/FvwmScript.c`, `modules/FvwmScript/Instructions.c`
- Impact: Deep expressions, nested blocks, many objects, or many launch arguments can corrupt memory rather than produce a parser error.
- Fix approach: Replace `BuffArg[6][20]`, `PileBloc[10]`, `TabObj[1000]`, `tabxobj[1000]`, and `TabArg[20]` with checked dynamic vectors. Centralize limits and reject input before incrementing an index.

**Generated parser sources are committed and built directly:**
- Issue: Meson compiles generated `modules/FvwmScript/scanner.c` and `modules/FvwmScript/script.c` directly instead of generating them from `modules/FvwmScript/scanner.l` and `modules/FvwmScript/script.y`. The committed outputs identify Flex 2.5.35 and Bison 2.3.
- Files: `modules/FvwmScript/meson.build`, `modules/FvwmScript/scanner.l`, `modules/FvwmScript/scanner.c`, `modules/FvwmScript/script.y`, `modules/FvwmScript/script.c`, `modules/FvwmScript/script.h`
- Impact: Source and generated files can drift, fixes may be applied to only one copy, and generated runtime code remains tied to old generator output.
- Fix approach: Generate parser artifacts reproducibly in Meson from pinned supported Flex/Bison versions, or document and CI-check an exact regeneration command and a clean generated diff.

**Module protocol remains native-layout and synchronous in places:**
- Issue: Module frames serialize native `Window` and `unsigned long` values and synchronous message masks can block the window manager waiting for a module. Comments describe deadlock workarounds and an unimplemented configurable timeout.
- Files: `fvwm/module_runtime.c`, `fvwm/module_list.c`, `fvwm/module_list.h`, `fvwm/module_interface.c`, `dev-docs/TODO.md`
- Impact: The protocol is ABI-sensitive, hard to version, and fragile around slow or unresponsive modules. A synchronous module can pause the main window-manager event loop until `moduleTimeout` expires.
- Fix approach: Define a versioned, length-delimited protocol with fixed-width fields and move all module I/O to nonblocking event-driven handling. Preserve the current input-size and output-queue limits during migration.

**Warnings are intentionally tolerated:**
- Issue: The default build uses `warning_level=2`, `werror=false`, and globally suppresses sign-comparison and unused-parameter warnings.
- Files: `meson.build`, `.github/workflows/ccpp.yml`
- Impact: New diagnostics can accumulate without failing a local or CI build; GCC CI promotes only a small set of LTO/aliasing diagnostics to errors.
- Fix approach: Remove suppressions incrementally by directory, raise warning coverage, and make the warning-clean subset fatal in CI before enabling project-wide `werror`.

## Known Bugs

**FvwmScript `GetOutput` can read and write out of bounds:**
- Symptoms: Command output can cause a crash or sanitizer failure. The buffer is exactly 32,000 bytes and `fread` is allowed to fill all 32,000 bytes without appending `\0`; subsequent loops scan until `\0`. A selected word is copied with unbounded `sscanf("%s")` into a 255-byte allocation, and a 255-byte command is copied with `strncpy` without guaranteed termination.
- Files: `modules/FvwmScript/Instructions.c`
- Trigger: Use the FvwmScript `GetOutput` function with output of at least 32,000 bytes, or request a whitespace-delimited output token longer than 254 bytes.
- Workaround: Keep command output below 32,000 bytes and individual selected words below 255 bytes; do not use `GetOutput` for arbitrary or binary output.

**FvwmScript accepts a 1,001st object before rejecting overflow:**
- Symptoms: Parser action `id` increments `nbobj`, but checks `nbobj > 1000`; valid storage indices are `0..999`. The 1,001st object therefore writes `(*tabobj)[1000]` out of bounds.
- Files: `modules/FvwmScript/script.y`, `modules/FvwmScript/script.c`, `modules/FvwmScript/types.h`, `modules/FvwmScript/FvwmScript.c`
- Trigger: Parse a script containing 1,001 `Widget` objects.
- Workaround: Limit each FvwmScript file to at most 1,000 widgets.

**FvwmScript argument copying has no `TabArg` bound check:**
- Symptoms: Module arguments are copied into `X11base.TabArg[20]` with a loop bounded only by `module->user_argc`.
- Files: `modules/FvwmScript/FvwmScript.c`, `modules/FvwmScript/types.h`, `libs/Module.c`
- Trigger: Start FvwmScript with enough user arguments to address index 20 or above.
- Workaround: Pass fewer than 20 total script argument slots, including the module/script bookkeeping entries.

**FvwmForm variable parsing can overflow or scan beyond input:**
- Symptoms: `ParseCommand` writes a variable name into static `char var[256]` without a capacity check and loops until `)`, `?`, or `!` without recognizing end-of-string.
- Files: `modules/FvwmForm/ParseCommand.c`, `modules/FvwmForm/FvwmForm.c`
- Trigger: Execute a form command containing an unterminated `$(` expression or a variable name longer than 255 non-space characters.
- Workaround: Keep substitutions well formed and variable names short; do not pass untrusted text as FvwmForm command syntax.

**FvwmMFL manual test client targets a nonexistent socket path:**
- Symptoms: `modules/FvwmMFL/test_fmd.pl` connects to `/tmp/fvwm_fmd.sock`, while the implementation creates a display-qualified socket below `$XDG_RUNTIME_DIR/fvwmmfl` or `/tmp/fvwmmfl-<uid>` and exports its path through `FVWMMFL_SOCKET`.
- Files: `modules/FvwmMFL/test_fmd.pl`, `modules/FvwmMFL/FvwmMFL.c`
- Trigger: Run `modules/FvwmMFL/test_fmd.pl` against the current FvwmMFL module.
- Workaround: Connect manually to the path in `FVWMMFL_SOCKET`.

## Security Considerations

**Fixed-size module action expansion buffer:**
- Risk: `module_expand_action` allocates exactly `MAX_MODULE_INPUT_TEXT_LEN` bytes, but copies ordinary input characters without checking the remaining capacity. Long configured actions can overwrite the heap before the terminating null byte is written.
- Files: `libs/Module.c`, `libs/defaults.h`, `modules/FvwmButtons/FvwmButtons.c`, `modules/FvwmButtons/dynamic.c`, `modules/FvwmIconMan/functions.c`
- Current mitigation: Variable substitutions check remaining capacity, and module-to-core commands are capped at 1,000 bytes in `fvwm/module_runtime.c`; direct configured action strings are not capped by `module_expand_action` itself.
- Recommendations: Allocate from `strlen(in_action)` plus checked expansion growth, or use a growable builder. Add ASan regression tests with long plain text and long substitutions.

**Unbounded X property retrieval:**
- Risk: `atom_get` requests up to `0x7fffffff` 32-bit units from an X property, then computes `num_ret * asize + 1` without checked arithmetic and narrows the byte count to `int`. A client on the same X server can set large EWMH properties and cause excessive allocation, integer overflow, process exit on allocation failure, or memory corruption.
- Files: `fvwm/ewmh.c`, `fvwm/ewmh_icons.c`, `libs/ImageSafety.c`
- Current mitigation: Decoded EWMH icons are constrained to 4,096 pixels per dimension and 16 million pixels in `fvwm/ewmh_icons.c`; the property is fetched in full before those checks run.
- Recommendations: Fetch a bounded prefix, validate `bytes_after`, use checked `size_t` multiplication, and impose per-atom byte limits before allocation.

**FvwmMFL is a same-user command-execution boundary:**
- Risk: Any process able to connect to the Unix socket can send lines that reach `SendText` and therefore invoke FVWM commands. Input length, client count, and each bufferevent's queued output are not capped, enabling same-user memory exhaustion or command execution through a compromised peer process.
- Files: `modules/FvwmMFL/FvwmMFL.c`
- Current mitigation: The socket directory must be owned by the current UID with no group/other permissions, and the socket is changed to mode `0600`. Display-derived names are sanitized.
- Recommendations: Keep the filesystem checks, set maximum input/output watermarks, disconnect slow clients, cap line size and client count, and document that possession of the socket grants FVWM command authority.

**Shell execution is an intentional but broad trust surface:**
- Risk: FVWM configuration and module scripts can execute shell text through `Exec`, `PipeRead`, FvwmForm `!` commands, and FvwmScript `GetOutput`. Variable expansion into these commands can become shell injection if configuration incorporates untrusted window titles, form input, or external output.
- Files: `fvwm/builtins.c`, `fvwm/read.c`, `modules/FvwmForm/FvwmForm.c`, `modules/FvwmScript/Instructions.c`
- Current mitigation: These features run with the desktop user's privileges and are configuration-driven; they are not exposed as a network service by the core.
- Recommendations: Treat config, module, and socket input as executable code. Prefer argv-based execution where shell semantics are not required, add explicit shell-escaping helpers, and mark unsafe interpolation points in user documentation.

**Build hardening is useful but incomplete as a safety net:**
- Risk: Memory-unsafe paths remain exploitable despite compiler defenses, and hardening availability varies by platform/compiler.
- Files: `meson.build`, `.github/workflows/ccpp.yml`
- Current mitigation: PIE, `-fstack-protector-strong`, Linux stack-clash protection, `_FORTIFY_SOURCE` when optimized, and ASan/UBSan CI are configured.
- Recommendations: Keep those defenses, add linker RELRO/now checks where supported, and expand sanitizer execution to parser, X-property, and module-facing tests rather than relying on three small test binaries.

## Performance Bottlenecks

**Busy-waiting in MiniScroll:**
- Problem: Mouse autorepeat allocates a `timeval` and repeatedly calls `gettimeofday` in a tight loop for roughly 133-267 ms.
- Files: `modules/FvwmScript/Widgets/MiniScroll.c`
- Cause: Delay handling is implemented as polling rather than a timer or blocking wait.
- Improvement path: Use libevent timers, `nanosleep`, or event-loop deadlines and keep processing X/module events while waiting.

**Polling sleeps in interactive paths:**
- Problem: Menu, move/resize, animation, and module code contain many `usleep` calls; some are inside event-processing loops.
- Files: `fvwm/menus.c`, `fvwm/events.c`, `fvwm/move_resize.c`, `modules/FvwmAnimate/FvwmAnimate.c`, `modules/FvwmButtons/FvwmButtons.c`
- Cause: Timing and retry behavior is interleaved with synchronous loops instead of represented as scheduled event-loop work.
- Improvement path: Move delays to `fvwm/schedule.c` or libevent callbacks, and retain the fairness budgets in `fvwm/event_loop_policy.c` for deferred work.

**FvwmMFL repeats serialization per subscriber and has no backpressure policy:**
- Problem: Every interested client causes a fresh packet-to-cJSON conversion, string allocation, and bufferevent write. Slow clients can accumulate queued output.
- Files: `modules/FvwmMFL/FvwmMFL.c`, `libs/cJSON.c`
- Cause: `handle_packet` is called inside the client loop, and no output high-water mark disconnects lagging consumers.
- Improvement path: Serialize each FVWM packet once, share the immutable payload across matching clients, configure write watermarks, and cap lag per client.

**Large EWMH properties are fetched before validation:**
- Problem: A large `_NET_*` property can consume memory and block the single event thread before icon-size validation occurs.
- Files: `fvwm/ewmh.c`, `fvwm/ewmh_icons.c`, `libs/ImageSafety.c`
- Cause: `atom_get` requests the entire property in one `XGetWindowProperty` call.
- Improvement path: Read bounded chunks, validate declared structures incrementally, and reject properties over atom-specific byte budgets.

## Fragile Areas

**Main X11 event loop and module lifecycle:**
- Files: `fvwm/events.c`, `fvwm/module_interface.c`, `fvwm/module_list.c`, `fvwm/module_runtime.c`, `fvwm/event_loop_policy.c`
- Why fragile: X events, signals, scheduled commands, module reads/writes, recursive command execution, startup completion, and delayed destruction share one thread and multiple queues. Synchronization comments in `fvwm/module_list.c` document deadlock-sensitive behavior.
- Safe modification: Keep descriptor polling between bounded work batches, pin module inputs until command disposal, and run the tests in `tests/test_event_loop_policy.c` and `tests/test_module_runtime.c` after every lifecycle change.
- Test coverage: Queue fairness and fragmented module frames are covered; real child processes, X events, synchronous masks, timeouts, restart, and signal races are not covered in `tests/meson.build`.

**FvwmScript parser and runtime:**
- Files: `modules/FvwmScript/script.y`, `modules/FvwmScript/script.c`, `modules/FvwmScript/scanner.l`, `modules/FvwmScript/scanner.c`, `modules/FvwmScript/Instructions.c`, `modules/FvwmScript/FvwmScript.c`
- Why fragile: Generated and source parsers coexist, runtime values are encoded into `long`, fixed stacks have no complete bounds policy, and parser actions perform manual allocation and global mutation.
- Safe modification: Change `script.y`/`scanner.l` first, regenerate outputs reproducibly, add explicit bounds before every index increment, and test malformed/deep scripts under ASan/UBSan.
- Test coverage: No automated FvwmScript test is registered in `tests/meson.build`.

**Style, menu, border, and geometry behavior:**
- Files: `fvwm/style.c`, `fvwm/menus.c`, `fvwm/borders.c`, `fvwm/move_resize.c`, `fvwm/placement.c`, `fvwm/virtual.c`
- Why fragile: The files are large, share `FvwmWindow` and screen state through macros, and combine policy with X side effects. `dev-docs/TODO.md` identifies the struct-access macro layer and style system as cleanup targets.
- Safe modification: Extract pure calculations first, preserve old command semantics with table-driven tests, and validate behavior in a nested X server before changing shared flags or geometry ordering.
- Test coverage: No automated menu, style, placement, multi-monitor, focus, or move/resize test is registered in `tests/meson.build`.

**FvwmForm command substitution:**
- Files: `modules/FvwmForm/ParseCommand.c`, `modules/FvwmForm/FvwmForm.c`
- Why fragile: A recursive parser uses static buffers and returns shared storage; callers can synchronously execute the expanded result through `system`.
- Safe modification: Replace the static parser buffer with an owned result object, enforce nesting/name limits, and separate expansion from execution.
- Test coverage: No automated FvwmForm parser or command test is registered in `tests/meson.build`.

## Scaling Limits

**Per-module queues:**
- Current capacity: Each module output queue is limited to 8 MiB, each incoming command text is limited to 1,000 bytes, and event-loop batches process 64 module commands or X events.
- Limit: A module exceeding 8 MiB is killed; long input frames are rejected; sustained work is deliberately deferred after 64 items.
- Scaling path: Keep bounded queues, make thresholds configurable with global aggregate caps, expose queue/kill metrics, and document limits from `fvwm/module_list.h`, `libs/defaults.h`, and `fvwm/event_loop_policy.h`.

**FvwmScript capacity:**
- Current capacity: Storage is fixed at 1,000 widgets, 1,000 IDs, 20 script arguments, 10 nested block slots, 6 argument layers, and 20 values per layer.
- Limit: Some limits produce explicit errors, while others overrun arrays because checks are absent or off by one.
- Scaling path: Use checked dynamic vectors and impose one documented resource budget in `modules/FvwmScript/script.y`, `modules/FvwmScript/types.h`, and `modules/FvwmScript/FvwmScript.c`.

**FvwmMFL clients:**
- Current capacity: No client-count, input-byte, or queued-output limit is defined.
- Limit: Capacity is bounded only by file descriptors and process memory; one slow or flooding same-user client can degrade all FvwmMFL clients.
- Scaling path: Add listener/client limits and bufferevent watermarks in `modules/FvwmMFL/FvwmMFL.c`, plus counters for dropped/disconnected clients.

**Single-threaded core:**
- Current capacity: X handling, command execution, scheduling, and module dispatch run through the single loop in `fvwm/events.c`.
- Limit: Any blocking shell/config operation, synchronous module wait, polling sleep, or expensive property conversion pauses desktop responsiveness.
- Scaling path: Keep X state single-threaded, but move subprocess I/O and independent parsing/serialization behind nonblocking descriptors or workers with immutable messages.

## Dependencies at Risk

**Vendored cJSON 1.7.17:**
- Risk: `libs/cJSON.c` and `libs/cJSON.h` are copied into the repository, while Dependabot covers only GitHub Actions and Go modules. C library fixes are not discovered automatically.
- Impact: FvwmMFL and core JSON output compile the vendored parser/serializer into `libfvwm3`; missed upstream fixes affect all users of those paths.
- Migration plan: Prefer a system dependency with a minimum supported version or track the vendored source through an automated upstream-version/security check. Record local modifications before replacing `libs/cJSON.c` and `libs/cJSON.h`.

**Committed Flex/Bison runtime output:**
- Risk: `modules/FvwmScript/scanner.c` identifies Flex 2.5.35 and `modules/FvwmScript/script.c` identifies Bison 2.3; generated output is not covered by a package update workflow.
- Impact: Parser-runtime defects and generator incompatibilities require manual regeneration, and source/output drift is easy to miss.
- Migration plan: Generate from `modules/FvwmScript/scanner.l` and `modules/FvwmScript/script.y` in a pinned build/maintenance job, then verify generated diffs in CI.

**Vendored Go module tree:**
- Risk: FvwmPrompt builds with `-mod=vendor` from 456 tracked vendor files, so `go.mod` updates must be followed by a synchronized vendor refresh.
- Impact: A changed `bin/FvwmPrompt/go.mod` without matching `bin/FvwmPrompt/vendor/` can leave builds using stale source or fail vendor consistency checks.
- Migration plan: Keep Dependabot coverage in `.github/dependabot.yml`, require `go mod vendor` consistency in CI, and add a direct FvwmPrompt test/build job for `bin/FvwmPrompt/meson.build`.

## Missing Critical Features

**Wayland backend:**
- Problem: The implementation and required dependencies are X11/XRandR based; Wayland is only an open backend item.
- Blocks: Running FVWM3 as a native Wayland compositor rather than through an X compatibility environment. Files: `dev-docs/TODO.md`, `meson.build`, `fvwm/fvwm3.c`.

**Actionable configuration error locations:**
- Problem: Config parsing does not consistently report source file and line number; this remains an explicit parser task.
- Blocks: Fast diagnosis of malformed nested reads and module configuration. Files: `dev-docs/TODO.md`, `fvwm/read.c`, `fvwm/cmdparser_old.c`.

**Versioned module IPC:**
- Problem: Module communication uses native binary layouts and shell-like command text without protocol negotiation.
- Blocks: ABI-independent external modules, robust remote tooling, and safe incremental protocol evolution. Files: `dev-docs/TODO.md`, `libs/Module.c`, `fvwm/module_runtime.c`, `fvwm/module_list.h`.

## Test Coverage Gaps

**X11 behavior and end-to-end window management:**
- What's not tested: Window adoption, focus, stacking, EWMH/ICCCM properties, menus, decorations, placement, multi-monitor changes, restart, and interactive move/resize behavior against an X server.
- Files: `fvwm/add_window.c`, `fvwm/focus.c`, `fvwm/stack.c`, `fvwm/ewmh.c`, `fvwm/menus.c`, `fvwm/placement.c`, `fvwm/move_resize.c`, `tests/meson.build`, `.github/workflows/ccpp.yml`
- Risk: Regressions in the primary user-visible behavior compile successfully and pass all three current test binaries.
- Priority: High

**Parser and string-safety boundaries:**
- What's not tested: Long actions in `module_expand_action`, malformed FvwmForm substitutions, deep/large FvwmScript inputs, 32,000-byte `GetOutput`, and generated-parser/source consistency.
- Files: `libs/Module.c`, `modules/FvwmForm/ParseCommand.c`, `modules/FvwmScript/Instructions.c`, `modules/FvwmScript/script.y`, `tests/meson.build`
- Risk: Heap/static-buffer corruption remains outside ASan/UBSan coverage because sanitizer CI executes only the registered tests.
- Priority: High

**FvwmMFL protocol, authorization, and backpressure:**
- What's not tested: Socket path/permission creation, command forwarding, partial and multiple lines, large input, client disconnects, slow consumers, and multiple subscribers. The only helper is the stale unregistered `modules/FvwmMFL/test_fmd.pl`.
- Files: `modules/FvwmMFL/FvwmMFL.c`, `modules/FvwmMFL/test_fmd.pl`, `tests/meson.build`
- Risk: Security-boundary and resource-exhaustion regressions can ship unnoticed.
- Priority: High

**Module synchronization and process lifecycle:**
- What's not tested: Real fork/exec modules, synchronous message acknowledgements, timeout killing, signal interruption, startup modules, and restart cleanup.
- Files: `fvwm/module_list.c`, `fvwm/module_interface.c`, `fvwm/events.c`, `tests/test_module_runtime.c`, `tests/test_event_loop_policy.c`
- Risk: Deadlocks, leaked descriptors, use-after-free, or desktop stalls appear only in a live session.
- Priority: High

**FvwmPrompt and Perl libraries:**
- What's not tested: No project-owned `*_test.go` exists beside `bin/FvwmPrompt/FvwmPrompt.go`, and no Perl `.t` suite covers `perllib/`.
- Files: `bin/FvwmPrompt/FvwmPrompt.go`, `bin/FvwmPrompt/go.mod`, `perllib/FVWM/Module.pm.in`, `perllib/FVWM/Tracker.pm`, `tests/meson.build`
- Risk: CLI protocol, terminal handling, and Perl compatibility regressions are detected only manually or by downstream users.
- Priority: Medium

**Coverage measurement and fuzzing:**
- What's not tested: No project coverage target, minimum threshold, corpus, or fuzz harness is configured for config parsers, module frames, X properties, images, or JSON conversion.
- Files: `tests/meson.build`, `.github/workflows/ccpp.yml`, `fvwm/module_runtime.c`, `modules/FvwmScript/scanner.l`, `modules/FvwmForm/ParseCommand.c`, `libs/ImageSafety.c`
- Risk: The three focused tests provide no visibility into unexecuted branches across roughly 193,000 non-vendored source lines.
- Priority: Medium

---

*Concerns audit: 2026-08-29*
