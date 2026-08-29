# Coding Conventions

**Analysis Date:** 2026-08-29

## Naming Patterns

**Files:**
- Put core C implementation and header files in lowercase snake case, as in `fvwm/event_loop_policy.c`, `fvwm/event_loop_policy.h`, `fvwm/module_runtime.c`, and `fvwm/module_list.h`.
- Reserve an initial `F` in `libs/` filenames for wrappers around third-party libraries or FVWM-facing abstractions, as documented in `dev-docs/DEVELOPERS.md`; examples include `libs/FShape.c`, `libs/FBidi.c`, and `libs/FEvent.c`. Do not apply that prefix to ordinary helpers such as `libs/fqueue.c`.
- Name loadable module directories and primary entry files in `Fvwm<Name>` form, as in `modules/FvwmPager/FvwmPager.c` and `modules/FvwmButtons/FvwmButtons.c`; keep subsidiary files descriptive and lowercase, such as `modules/FvwmPager/fpmonitor.c`.
- Name standalone C tests `test_<unit>.c`, as in `tests/test_image_safety.c`, `tests/test_module_runtime.c`, and `tests/test_event_loop_policy.c`.
- Use `.pm` files and namespace-shaped directories for Perl packages, as in `perllib/FVWM/Module/Toolkit.pm`; generated/configured executable scripts use `.in`, as in `bin/fvwm-menu-desktop.in`.
- Treat `bin/FvwmPrompt/vendor/` as third-party code. Do not restyle it to match project-owned C, Go, or Perl files; its source list is consumed through `bin/FvwmPrompt/meson.build`.

**Functions:**
- Use lower snake case for new C helpers and APIs, as in `fvwm/event_loop_policy.c` (`fvwm_bounded_queue_drain`) and `fvwm/module_runtime.c` (`module_output_queue_reserve`). Mark file-private helpers `static`, as in `module_list_insert()` in `fvwm/module_runtime.c`.
- Preserve established subsystem prefixes and legacy exported spellings instead of renaming public APIs: `EWMH_DoUpdateWmIcon()` in `fvwm/ewmh_icons.c`, `CMD_Send_ConfigInfo()` in `fvwm/modconf.c`, and `FImageSizeIsSafe()` in `libs/ImageSafety.c` are compatibility-facing names.
- Name test cases `static void test_<behavior>(void)`, and invoke them explicitly from `main()`, following `tests/test_event_loop_policy.c` and `tests/test_image_safety.c`.
- Follow Go conventions in `bin/FvwmPrompt/FvwmPrompt.go`: lower camel case for unexported functions and variables (`handleInput`, `cmdLineArgs`) and upper camel case only for exported fields needed by encoders (`ConnectionProfile`, `VersionInfo`).
- Use snake-case Perl methods and lexical names, such as `show_message`, `$error_msg`, and `$_dialog_tool` in `perllib/FVWM/Module/Toolkit.pm`; use `FVWM::...` package namespaces that match the path.

**Variables:**
- Use descriptive lowercase snake case for new C locals and fields, as in `pixel_count` in `libs/ImageSafety.c` and `consecutive_events` in `fvwm/event_loop_policy.c`.
- Keep declarations close to the smallest useful block when matching modern C11 code such as `libs/ImageSafety.c`; in legacy files such as `fvwm/fvwm3.c`, retain the surrounding declaration-before-statements style to avoid unrelated churn.
- Preserve domain types: core/X11 code uses `Bool`, `True`, `False`, `Window`, and `None` in files such as `fvwm/module_runtime.c`, while self-contained standard-C helpers use `bool`, `true`, and `false` in `libs/ImageSafety.c`.
- Avoid introducing new unprefixed globals. Existing globals such as `Scr`, `dpy`, and `fvwm_userdir` in `fvwm/fvwm3.c` reflect legacy interfaces; new state should be passed through parameters or kept `static` as in `fvwm/module_runtime.c`.

**Types:**
- Use lowercase domain names and `_t` suffixes where the subsystem already does so, such as `fvwm_run_state_t` in `fvwm/fvwm3.c`, `fvwm_queue_consumer_t` in `fvwm/event_loop_policy.h`, and `cond_rc_t` in `fvwm/condrc.h`.
- Struct typedefs may use either anonymous structs (`fvwm_queue_drain_state` in `fvwm/event_loop_policy.h`) or named tags needed for self-reference (`fmodule_store` in `fvwm/module_list.h`); follow the local header's pattern.
- Use uppercase snake case for enum members, constants, and macros, as in `FMODULE_INPUT_READY` and `MODULE_QUEUE_MAX_BYTES` in `fvwm/module_list.h`.
- Use uppercase include guards derived from subsystem and filename, such as `FVWM_EVENT_LOOP_POLICY_H` in `fvwm/event_loop_policy.h` and `FVWMLIB_QUEUE_H` in `libs/fqueue.h`.

## Code Style

**Formatting:**
- Format project-owned C and headers with the repository root `.clang-format`. It is LLVM-based with Mozilla brace placement, tabs of width 8, an 80-column limit, right-aligned pointers, sorted includes, and no one-line functions, blocks, conditionals, or loops.
- Use tabs for indentation in `*.c` and `*.h`, spaces only where alignment requires them, UTF-8, LF endings, no trailing whitespace, and a final newline, as specified by `.editorconfig`.
- Use four spaces for Meson files, as specified for `meson.build` in `.editorconfig`; examples are `tests/meson.build` and `bin/FvwmPrompt/meson.build`.
- Put opening braces on the next line for C functions and control blocks, as demonstrated in `fvwm/event_loop_policy.c` and `tests/test_module_runtime.c`.
- Write pointer declarations with the `*` attached to the variable, for example `fmodule *module` in `fvwm/module_runtime.c`, matching `.clang-format`'s `PointerAlignment: Right`.
- Keep lines at or below 80 columns and indent continuations by one tab/four visual continuation spaces, following calls in `tests/test_event_loop_policy.c` and settings in `.clang-format`.
- Use `gofmt` conventions for `bin/FvwmPrompt/FvwmPrompt.go`; its imports are grouped into standard-library and third-party blocks. No repository-specific Go formatter configuration exists outside the source and `bin/FvwmPrompt/go.mod`.
- Match the surrounding Perl style because no Perl formatter configuration is present. Package modules such as `perllib/FVWM/Module/Toolkit.pm` use tabs, braces on the same line as control statements, `use strict`, and lexical `my` variables.

**Linting:**
- Treat the Linux kernel coding guide as the human style baseline, per `dev-docs/DEVELOPERS.md`, while `.clang-format` and `.editorconfig` provide the executable whitespace rules.
- Build as C11 with Meson warning level 2, as configured in `meson.build`; project-wide `-fno-common`, `-Wno-sign-compare`, and `-Wno-unused-parameter` flags also come from `meson.build`.
- Do not rely on warnings being fatal locally because `werror=false` is set in `meson.build`. The GCC CI leg adds selected `-Werror` flags for ODR, LTO type mismatch, and strict aliasing in `.github/workflows/ccpp.yml`.
- No project-owned `clang-tidy`, `cppcheck`, `golangci-lint`, or automated format-check job is configured; `.github/workflows/ccpp.yml` validates compilation and tests. The `.golangci.yml` under `bin/FvwmPrompt/vendor/github.com/sirupsen/logrus/` belongs to vendored code.
- Codacy is an external review signal linked from `README.md` and referenced by `.github/PULL_REQUEST_TEMPLATE.md`; it does not replace the repository's Meson build/test gate in `.github/workflows/ccpp.yml`.

## Import Organization

**Order:**
1. Place `#include "config.h"` as the first non-comment line in production `.c` files, as required by `dev-docs/DEVELOPERS.md` and demonstrated in `fvwm/module_runtime.c` and `modules/FvwmButtons/dynamic.c`.
2. Include platform and C standard-library headers with angle brackets, grouped after `config.h`, as in `fvwm/module_runtime.c` (`<errno.h>`, `<stdlib.h>`, `<string.h>`, `<unistd.h>`).
3. Include shared project headers using repository-relative quoted paths, followed by same-directory headers, as in `fvwm/module_runtime.c` (`"libs/log.h"`, `"libs/safemalloc.h"`, then `"module_list.h"`).
4. In headers, include the standard types and dependent project headers required by the public declarations, as in `fvwm/module_list.h`; do not depend on an includer's accidental transitive imports.

**Path Aliases:**
- C code uses source-root-relative quoted includes such as `"libs/fqueue.h"` and `"fvwm/execcontext.h"`, enabled by include directories in `meson.build`; same-directory headers may use short names such as `"event_loop_policy.h"` in `fvwm/event_loop_policy.c`.
- Go imports use the module path `github.com/fvwmorg/FvwmPrompt` from `bin/FvwmPrompt/go.mod`; external packages are imported by canonical paths in `bin/FvwmPrompt/FvwmPrompt.go`.
- Perl modules use namespace imports corresponding to `perllib/`, such as `use FVWM::Module;` in `perllib/FVWM/Module/Toolkit.pm`.

## Error Handling

**Patterns:**
- Return explicit status values for recoverable C failures: `NULL`, `False`/`false`, `-1`, or a domain enum. `module_receive()` returns `FMODULE_INPUT_INCOMPLETE`, `FMODULE_INPUT_READY`, or `FMODULE_INPUT_ERROR` in `fvwm/module_runtime.c`; callers switch behavior through assertions in `tests/test_module_runtime.c`.
- Initialize output parameters before parsing and leave them in a safe state on failure, following `FEwmhIconFindBest()` in `libs/ImageSafety.c`, which clears `pixel_start`, `best_width`, and `best_height`.
- Validate sizes before arithmetic or allocation, as in `FImageSizeIsSafe()` in `libs/ImageSafety.c` and `module_output_queue_reserve()` in `fvwm/module_runtime.c`.
- Use a single cleanup/error label when multiple branches share teardown, as in the `err:` path of `module_receive()` in `fvwm/module_runtime.c`; make ownership and close/free behavior explicit.
- Use `fxmalloc()`, `fxcalloc()`, `fxrealloc()`, and `fxstrdup()` from `libs/safemalloc.c` where allocation failure is unrecoverable. These helpers terminate through `err()`/`errx()` rather than returning `NULL`.
- Log recoverable diagnostics with `fvwm_debug(__func__, ...)`, as in `fvwm/module_runtime.c`; do not silently swallow malformed external input when a useful diagnostic can be emitted.
- In Go, return from benign terminal conditions and use the shared `logrus.Logger` for fatal startup/connection failures, following `bin/FvwmPrompt/FvwmPrompt.go`. Check returned errors at call boundaries rather than discarding them in new code.
- In Perl, enable `use strict` and `use warnings` for new scripts, then use `die` for unrecoverable setup errors and `warn`/`STDERR` for nonfatal diagnostics, following `modules/FvwmMFL/test_fmd.pl` and `perllib/FVWM/Module/Toolkit.pm`.

## Logging

**Framework:** `fvwm_debug()` from `libs/log.h` for C, Logrus for `bin/FvwmPrompt/FvwmPrompt.go`, and `STDERR`/`warn` for Perl modules such as `perllib/FVWM/Module/Toolkit.pm`.

**Patterns:**
- Pass `__func__` as the first argument to C diagnostics so log records identify their origin, as in `fvwm/module_runtime.c` and `fvwm/ewmh_icons.c`.
- Use `fprintf(stderr, ...)` for command-line usage and direct user-facing errors, as in `fvwm/fvwm3.c`; use `fvwm_debug()` for runtime diagnostics governed by FVWM logging state in `libs/log.c`.
- Do not add unconditional debug prints. Existing compile-time/runtime debug switches in files such as `fvwm/events.c` are subsystem-specific and should be preserved rather than copied into new code.
- Keep fatal Go logging at process-boundary failures, such as `net.Dial` and readline initialization in `bin/FvwmPrompt/FvwmPrompt.go`; ordinary control flow should not call `log.Fatal`.

## Comments

**When to Comment:**
- Explain invariants, ownership, compatibility behavior, and non-obvious policy rather than restating operations. Examples include queue locking semantics in `libs/fqueue.c` and input pinning comments in `fvwm/module_list.h`.
- Preserve the GPL notice at the beginning of new C source files and scripts, but not headers, as required by `dev-docs/DEVELOPERS.md`.
- Legacy C files use section banners such as those in `fvwm/fvwm3.c` and `modules/FvwmButtons/dynamic.c`; maintain them when editing those files, but do not add empty banners to compact files such as `fvwm/event_loop_policy.c`.
- Document every user-visible feature and option in the AsciiDoc manual under `doc/`, as required by `dev-docs/DEVELOPERS.md`; module documentation examples include `doc/FvwmPager.adoc` and `doc/FvwmButtons.adoc`.

**JSDoc/TSDoc:**
- Not applicable. Public C APIs use concise block comments near declarations, as in `fvwm/module_list.h`; Perl library documentation uses POD after `__END__`, as in `perllib/FVWM/Module/Toolkit.pm`.

## Function Design

**Size:** Keep policy and parsing helpers focused and independently testable, as demonstrated by `fvwm/event_loop_policy.c` and `libs/ImageSafety.c`. Split reusable logic out of X11-heavy files before testing it rather than growing monolithic handlers such as `fvwm/ewmh_icons.c`.

**Parameters:**
- Pass state explicitly through pointers and callback context, as in `fvwm_bounded_queue_drain()` in `fvwm/event_loop_policy.c`; use `const` for read-only buffers and strings, as in `FEwmhIconFindBest()` in `libs/ImageSafety.c`.
- Use output parameters only when multiple values must be returned, initialize them defensively, and make success/failure visible in the return value, following `FEwmhIconFindBest()` in `libs/ImageSafety.c`.
- Use `void` in parameter lists for no-argument C functions, as in `module_create(void)` in `fvwm/module_runtime.c` and every test case in `tests/test_image_safety.c`.

**Return Values:**
- Use boolean results for predicates and capacity checks, status enums for multi-state operations, pointers/`NULL` for lookup or ownership transfer, and `0`/negative integer conventions only where the surrounding API already uses them; examples are in `libs/ImageSafety.c`, `fvwm/module_runtime.c`, and `libs/System.c`.
- Keep cleanup idempotent where practical: `module_destroy(NULL)` and `module_input_discard(NULL)` return safely in `fvwm/module_runtime.c`.

## Module Design

**Exports:** Declare public C functions and types in a matching header such as `fvwm/event_loop_policy.h`; keep implementation-only helpers and module state `static` in the `.c` file, as in `fvwm/module_runtime.c`. Use access macros only where an established subsystem API already exposes them, as in `fvwm/module_list.h`.

**Barrel Files:** Not used. Include the narrow header that owns the needed declaration, such as `"libs/ImageSafety.h"` in `fvwm/ewmh_icons.c`; broad aggregation headers such as `libs/fvwmlib.h` are legacy interfaces and should not become the default for new isolated helpers.

---

*Convention analysis: 2026-08-29*
