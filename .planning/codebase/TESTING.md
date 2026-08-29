# Testing Patterns

**Analysis Date:** 2026-08-29

## Test Framework

**Runner:**
- Meson test runner, with Meson `>=1.5.1` required by the root `meson.build` and all test targets registered in `tests/meson.build`.
- Three project-owned automated test executables are registered: `image-safety`, `module-runtime`, and `event-loop-policy` in `tests/meson.build`.
- Test executables are plain C programs with their own `main()` functions in `tests/test_image_safety.c`, `tests/test_module_runtime.c`, and `tests/test_event_loop_policy.c`; no third-party C unit-test framework is linked.

**Assertion Library:**
- Standard C `<assert.h>`, used directly by all three files under `tests/`. Each target passes `-UNDEBUG` in `tests/meson.build`, ensuring assertions remain active even for configurations that define `NDEBUG`.

**Run Commands:**
```bash
meson setup builddir                         # Configure from the repository root (`meson.build`)
meson compile -C builddir                    # Build production and test executables
meson test -C builddir --print-errorlogs     # Run all registered tests, matching CI
meson test -C builddir image-safety          # Run one named test from `tests/meson.build`
# Watch mode: not configured in this repository
# Coverage command: not configured in this repository
```

- The convenience `./build` script compiles through Meson/Ninja but does not invoke `meson test`; run the test command separately after using `build`.

## Test File Organization

**Location:**
- Keep automated C tests in the separate top-level `tests/` directory and register each target in `tests/meson.build`.
- Compile the production unit under test directly into a small test executable where isolation is practical, as `tests/meson.build` does for `../libs/ImageSafety.c`, `../fvwm/module_runtime.c`, and `../fvwm/event_loop_policy.c`.
- Link `libfvwm3` and `all_found_deps` only when the unit needs shared FVWM/X types or symbols, as the `module-runtime` target does in `tests/meson.build`; the `image-safety` target intentionally has no project-library dependency.
- `modules/FvwmMFL/test_fmd.pl` is an unregistered manual smoke client for a running Unix socket service, not part of `meson test`.
- Vendored tests and CI files under `bin/FvwmPrompt/vendor/` belong to dependencies and are not FVWM3's test suite; project-owned `bin/FvwmPrompt/FvwmPrompt.go` has no `_test.go` companion.

**Naming:**
- Name test sources `test_<unit>.c`, test executables `test-<unit>`, and Meson test registrations `<unit>`, following all entries in `tests/meson.build`.
- Name individual cases `test_<behavior>()`, such as `test_ewmh_icon_rejects_malformed_data()` in `tests/test_image_safety.c` and `test_recursive_drain_shares_budget()` in `tests/test_event_loop_policy.c`.
- Name reusable test helpers by their action or assertion, such as `create_pipe_module()`, `make_frame()`, `write_all()`, and `assert_fd_closed()` in `tests/test_module_runtime.c`.

**Structure:**
```text
tests/
├── meson.build                 # Builds and registers every automated test
├── test_event_loop_policy.c    # Queue-drain and X-event policy unit tests
├── test_image_safety.c         # Image-size and EWMH icon-data unit tests
└── test_module_runtime.c       # Pipe framing, lifetime, and queue-limit tests
```

## Test Structure

**Suite Organization:**
```c
/* Pattern from `tests/test_image_safety.c`. */
static void test_image_dimensions(void)
{
	size_t pixels = 0;

	assert(FImageSizeIsSafe(1920, 1080, 32768,
		64U * 1024U * 1024U, &pixels));
	assert(pixels == 1920U * 1080U);
}

int main(void)
{
	test_image_dimensions();
	/* Call the remaining focused cases explicitly. */
	return 0;
}
```

**Patterns:**
- Arrange state with local stack variables and helper functions, invoke one production behavior, then assert return values and state transitions. `tests/test_event_loop_policy.c` initializes `consumer_state`, `fqueue`, and `fvwm_queue_drain_state` before each drain assertion.
- Keep each case independent. `tests/test_module_runtime.c` creates fresh pipes and `fmodule` objects for each lifetime scenario and explicitly closes/discards them before returning.
- Test both the direct result and observable side effects. `tests/test_module_runtime.c` asserts status enums, command contents, reference counts, queue byte counts, and file-descriptor closure.
- Group closely related boundary cases in one case when they share setup, as `test_ewmh_icon_rejects_malformed_data()` does in `tests/test_image_safety.c` for truncated, overflowing, zero-size, and trailing input.
- Use typed integer suffixes and `sizeof(array) / sizeof(array[0])` when constructing protocol data, following `tests/test_image_safety.c`.
- A failed `assert()` aborts the process; Meson treats the nonzero/abnormal exit as test failure and prints details with the CI command in `.github/workflows/ccpp.yml`.

**Setup:**
- No framework-level setup hook exists. Put common setup in a `static` helper, as `create_pipe_module()` does in `tests/test_module_runtime.c`.
- Initialize every field that a callback reads. `tests/test_event_loop_policy.c` assigns `queue`, `drain_state`, counters, recursion flags, and tokens explicitly in each case.

**Teardown:**
- No framework-level teardown hook exists. Release ownership in each case with production cleanup APIs such as `module_input_discard()`, `module_kill()`, `module_cleanup()`, and `close()`, as shown in `tests/test_module_runtime.c`.
- Verify cleanup behavior where it is part of the contract. `assert_fd_open()` and `assert_fd_closed()` in `tests/test_module_runtime.c` test deferred destruction rather than merely preventing leaks.

## Mocking

**Framework:** None. The automated tests use hand-built state, function-pointer collaborators, and real operating-system primitives in `tests/`.

**Patterns:**
```c
/* Callback fake from `tests/test_event_loop_policy.c`. */
static void consume(void *object, void *context)
{
	consumer_state *state = context;

	assert(object != NULL);
	state->consumed++;
	if (state->consumed < state->requeue_until)
	{
		fqueue_add_at_end(state->queue, &state->token);
	}
}
```

- Pass mutable context through an existing callback seam instead of introducing a mocking dependency, following `consume()` and `consumer_state` in `tests/test_event_loop_policy.c`.
- Use real `pipe()`, `fcntl()`, `read()`, `write()`, and `close()` behavior for module transport tests in `tests/test_module_runtime.c`; this gives deterministic integration with POSIX nonblocking I/O without starting FVWM or X11.
- Construct raw protocol bytes with `memcpy()` in `make_frame()` from `tests/test_module_runtime.c` instead of mocking the parser's internals.

**What to Mock:**
- Replace callbacks or pure policy inputs with local fakes when the production API already accepts them, as in `fvwm_bounded_queue_drain()` from `fvwm/event_loop_policy.h` and its fake consumer in `tests/test_event_loop_policy.c`.
- Extract pure validation/policy code from X11-heavy paths before testing it, as `libs/ImageSafety.c` is consumed by `fvwm/ewmh_icons.c` but tested without an X server in `tests/test_image_safety.c`.

**What NOT to Mock:**
- Do not mock small deterministic data structures such as `fqueue`; `tests/test_event_loop_policy.c` uses the production implementation through `libfvwm3`.
- Do not mock file descriptors for module framing and lifecycle behavior; use local pipes as in `tests/test_module_runtime.c`.
- Do not include implementation `.c` files with preprocessor substitution inside test sources. Declare the production source in the Meson target as done in `tests/meson.build`.

## Fixtures and Factories

**Test Data:**
```c
/* Table-like malformed inputs from `tests/test_image_safety.c`. */
const uint32_t truncated[] = {2, 2, 1, 2, 3};
const uint32_t overflow[] = {65536, 65536};
const uint32_t zero_size[] = {0, 1};
const uint32_t trailing_data[] = {1, 1, 1, 2};
```

- Prefer compact, case-local arrays and structs over shared mutable global fixtures, following all three files in `tests/`.
- Build protocol frames with a helper when layout is significant; `make_frame()` in `tests/test_module_runtime.c` serializes `Window`, size, command bytes, and continuation flag in production order.
- Use named production limits in fixtures instead of copied magic values, such as `FVWM_MODULE_COMMAND_BUDGET` in `tests/test_event_loop_policy.c`, `MAX_MODULE_INPUT_TEXT_LEN` in `tests/test_module_runtime.c`, and `MODULE_QUEUE_MAX_BYTES` in `tests/test_module_runtime.c`.

**Location:**
- Fixtures are local to the test source that owns them in `tests/`; there is no shared fixture directory, generated test-data directory, or factory library in `tests/`.

## Coverage

**Requirements:** No percentage threshold or coverage gate is defined in `meson.build`, `meson.options`, `tests/meson.build`, or `.github/workflows/ccpp.yml`.

**View Coverage:**
```bash
# No repository-defined coverage target or documented coverage command exists.
# `meson.options` does not add project-specific coverage controls.
```

- The suite exercises three extracted areas: image safety in `libs/ImageSafety.c`, module runtime in `fvwm/module_runtime.c`, and event-loop policy in `fvwm/event_loop_policy.c`, as mapped in `tests/meson.build`.
- No project-owned automated tests cover the single Go program `bin/FvwmPrompt/FvwmPrompt.go`, Perl libraries under `perllib/`, module implementations under `modules/`, or the main X11 event/UI paths under `fvwm/`.

## CI

**GitHub Actions:**
- `.github/workflows/ccpp.yml` runs on pushes and pull requests targeting `main` and gives the build job a 20-minute timeout.
- The build matrix in `.github/workflows/ccpp.yml` combines GCC and Clang with Debian/Ubuntu and Alpine build containers, producing four configure/build/test legs.
- Every matrix leg runs `meson setup builddir --buildtype=debugoptimized -Dhtmldoc=true -Dmandoc=true`, `meson compile -C builddir`, and `meson test -C builddir --print-errorlogs` in `.github/workflows/ccpp.yml`.
- The GCC matrix leg adds LTO and selected fatal diagnostics (`-Werror=odr`, `-Werror=lto-type-mismatch`, and `-Werror=strict-aliasing`) in `.github/workflows/ccpp.yml`; the Clang leg selects `lld`.
- A separate sanitizer job in `.github/workflows/ccpp.yml` uses GCC, a debug build, `-Db_sanitize=address,undefined`, and `-Dgolang=disabled`, then compiles and runs all Meson tests.
- The sanitizer job validates C code only because Go is disabled in `.github/workflows/ccpp.yml`; no `go test`, race-detector, Perl test, or shell test command is part of project CI.
- `.github/workflows/release.yml` builds documentation, Go support, and a release tarball through `meson dist`, but it does not run `meson test`; merge CI in `.github/workflows/ccpp.yml` is the automated test gate described by `dev-docs/DEVELOPERS.md`.

## Test Types

**Unit Tests:**
- Pure validation and selection logic is tested with direct inputs and output assertions in `tests/test_image_safety.c` against `libs/ImageSafety.c`.
- Pure queue/event scheduling policy is tested with a fake callback and production queue implementation in `tests/test_event_loop_policy.c` against `fvwm/event_loop_policy.c`.
- Add new low-dependency tests by extracting policy into a narrow `.c`/`.h` pair and registering a standalone executable in `tests/meson.build`, following these two targets.

**Integration Tests:**
- `tests/test_module_runtime.c` is a component-level integration test: it combines `fvwm/module_runtime.c`, `libfvwm3`, real POSIX pipes, nonblocking I/O, queue storage, and cleanup behavior.
- `modules/FvwmMFL/test_fmd.pl` manually connects to `/tmp/fvwm_fmd.sock` and writes an FVWM command; because it requires a running module and has no Meson registration or assertions, treat it as a developer smoke client rather than an automated integration test.

**E2E Tests:**
- Not used. No automated test starts the `fvwm3` executable, an X server, a window client, or a full module session; the registered inventory in `tests/meson.build` contains only the three component executables.

## Common Patterns

**Async Testing:**
```c
/* Byte-at-a-time nonblocking input from `tests/test_module_runtime.c`. */
for (i = 0; i < frame_size; i++)
{
	write_all(write_fd, frame + i, 1);
	if (i + 1 == frame_size)
	{
		assert(module_receive(module, &input) == FMODULE_INPUT_READY);
	}
	else
	{
		assert(module_receive(module, &input) ==
			FMODULE_INPUT_INCOMPLETE);
	}
}
```

- Exercise asynchronous boundaries deterministically without sleeps: feed a nonblocking pipe incrementally and assert incomplete/ready states, as in `tests/test_module_runtime.c`.
- Exercise reentrancy by invoking the production drain recursively from the fake callback and checking the shared budget, as in `test_recursive_drain_shares_budget()` in `tests/test_event_loop_policy.c`.
- Reset process-global module state through production cleanup after each case in `tests/test_module_runtime.c`; otherwise later cases can depend on execution order.

**Error Testing:**
```c
/* Invalid length handling from `tests/test_module_runtime.c`. */
size = MAX_MODULE_INPUT_TEXT_LEN + 1UL;
write_all(write_fd, header, sizeof(header));
assert(module_receive(module, &input) == FMODULE_INPUT_ERROR);
assert(input == NULL);
module_cleanup();
assert_fd_closed(read_fd);
```

- Assert the failure result, safe output state, and cleanup side effect together, following `test_invalid_frames_kill_module()` in `tests/test_module_runtime.c`.
- Cover zero, limit, just-over-limit, malformed/truncated, and resource-exhaustion boundaries, following `tests/test_image_safety.c` and `test_output_queue_accounting()` in `tests/test_module_runtime.c`.
- Include recovery-after-policy-boundary behavior, not just the boundary itself; `test_x_event_budget()` in `tests/test_event_loop_policy.c` asserts reset and subsequent delivery after polling.

---

*Testing analysis: 2026-08-29*
