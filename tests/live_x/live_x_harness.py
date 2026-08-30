#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see: <http://www.gnu.org/licenses/>

"""Own nested-X processes and evidence for FVWM live-X scenarios."""

from __future__ import annotations

import argparse
import ctypes
import ctypes.util
import json
import os
from pathlib import Path
import re
import selectors
import signal
import subprocess
import tempfile
import time


FAULT_ENV = "FVWM_TEST_XRR_FAIL_FILE"
PRELOAD_ENV = "LD_PRELOAD"
POLL_QUANTUM_SECONDS = 0.05
MAX_STATUS_LINE_BYTES = 1024 * 1024


class HarnessError(RuntimeError):
    """Raised when a bounded harness operation cannot complete."""


def build_parser() -> argparse.ArgumentParser:
    """Describe the stable live-X command-line contract."""
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument("--backend", choices=("xvfb", "xorg-dummy"))
    parser.add_argument(
        "--scenario",
        choices=("failed-query-recovery", "fault-injection-probe", "topology"),
        default=None,
    )
    parser.add_argument("--fvwm", type=Path)
    parser.add_argument("--wrapped-fvwm", type=Path)
    parser.add_argument("--fault-library", type=Path)
    parser.add_argument("--xorg-config", type=Path)
    parser.add_argument("--policy-header", type=Path)
    parser.add_argument("--artifacts-dir", type=Path)
    parser.add_argument("--check-workflow", type=Path)
    parser.add_argument("--self-test", action="store_true")
    return parser


def check_live_x_workflow(workflow_path: Path) -> None:
    """Validate syntax and the dedicated native-Linux live-X CI contract."""
    if not workflow_path.is_file():
        raise HarnessError(f"workflow does not exist: {workflow_path}")

    try:
        syntax = subprocess.run(
            [
                "ruby",
                "-e",
                'require "yaml"; YAML.parse_file(ARGV.fetch(0))',
                str(workflow_path),
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            text=True,
        )
    except FileNotFoundError as error:
        raise HarnessError("Ruby is required for workflow syntax validation") from error
    if syntax.returncode != 0:
        diagnostic = syntax.stderr.strip() or "Ruby Psych rejected the workflow"
        raise HarnessError(f"invalid workflow YAML: {diagnostic}")

    workflow = workflow_path.read_text()
    if "\t" in workflow:
        raise HarnessError("workflow must not contain tab indentation")

    job_matches = list(re.finditer(r"(?m)^  live-x:\s*$", workflow))
    if len(job_matches) != 1:
        raise HarnessError("workflow must contain exactly one live-x job")
    job_start = job_matches[0].start()
    next_job = re.search(
        r"(?m)^  [A-Za-z0-9_-]+:\s*$", workflow[job_start + 1 :]
    )
    job_end = (
        job_start + 1 + next_job.start() if next_job is not None else len(workflow)
    )
    job = workflow[job_start:job_end]

    if "matrix:" in job or "${{ matrix." in job:
        raise HarnessError("live-x job must be a dedicated non-matrix job")
    for native_linux_anchor in (
        "runs-on: blacksmith-4vcpu-ubuntu-2404",
        "image: fvwmorg/fvwm3-build:latest",
    ):
        if job.count(native_linux_anchor) != 1:
            raise HarnessError(
                f"live-x native Linux job anchor missing: {native_linux_anchor}"
            )
    required_packages = {
        "x11-xserver-utils",
        "xserver-xorg-core",
        "xserver-xorg-video-dummy",
        "xvfb",
    }
    installs = re.findall(r"(?m)^\s*apt-get install -y ([^\n]+)$", job)
    if (
        len(installs) != 1
        or len(installs[0].split()) != len(required_packages)
        or set(installs[0].split()) != required_packages
    ):
        raise HarnessError("live-x job must install exactly the four audited X packages")

    commands = (
        "meson setup builddir -Dlive-x-tests=true -Dgolang=disabled",
        "meson compile -C builddir",
        "meson test -C builddir --suite live-x --print-errorlogs",
    )
    for command in commands:
        if job.count(command) != 1:
            raise HarnessError(f"live-x job must run exactly once: {command}")

    artifact_contract = (
        "if: failure()",
        "uses: actions/upload-artifact@v4",
        "builddir/tests/live-x-artifacts/",
        "builddir/meson-logs/",
        "if-no-files-found: error",
        "retention-days: 7",
    )
    for requirement in artifact_contract:
        if job.count(requirement) != 1:
            raise HarnessError(
                f"live-x failure artifact contract missing: {requirement}"
            )

    preserved_anchors = (
        "os: [blacksmith-4vcpu-ubuntu-2404, alpine]",
        "compiler: [gcc, clang]",
        "-Dhtmldoc=true -Dmandoc=true",
        "  sanitizers:",
        "-Db_sanitize=address,undefined",
    )
    for anchor in preserved_anchors:
        if workflow.count(anchor) != 1:
            raise HarnessError(f"existing CI anchor changed or missing: {anchor}")


def sanitized_environment(
    private_tmpdir: Path, overrides: dict[str, str] | None = None
) -> dict[str, str]:
    """Return the child environment without inherited fault controls."""
    environment = dict(os.environ)
    environment.pop(FAULT_ENV, None)
    environment.pop(PRELOAD_ENV, None)
    environment["TMPDIR"] = str(private_tmpdir)
    if overrides is not None:
        environment.update(overrides)
    return environment


class ManagedProcess:
    """A child process whose session, log, and bounded teardown are owned."""

    def __init__(
        self,
        name: str,
        argv: list[str],
        log_path: Path,
        process: subprocess.Popen[bytes],
        log_file: object,
    ) -> None:
        self.name = name
        self.argv = argv
        self.log_path = log_path
        self.process = process
        self._log_file = log_file
        self._closed = False

    @classmethod
    def start(
        cls,
        name: str,
        argv: list[str],
        log_path: Path,
        environment: dict[str, str],
        pass_fds: tuple[int, ...] = (),
    ) -> "ManagedProcess":
        """Launch an argv vector without a shell in a fresh process session."""
        if type(argv) is not list or not argv:
            raise ValueError("child argv must be a non-empty list")
        if any(type(argument) is not str or "\0" in argument for argument in argv):
            raise ValueError("child argv entries must be NUL-free strings")
        if any(type(fd) is not int or fd < 0 for fd in pass_fds):
            raise ValueError("pass_fds entries must be nonnegative integers")
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_file = log_path.open("wb")
        try:
            process = subprocess.Popen(
                argv,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
                shell=False,
                env=environment,
                start_new_session=True,
                pass_fds=pass_fds,
            )
        except BaseException:
            log_file.close()
            raise
        return cls(name, list(argv), log_path, process, log_file)

    def poll(self) -> int | None:
        """Return the child status without blocking."""
        return self.process.poll()

    def wait_until_exit(self, deadline: float) -> int:
        """Wait no later than an absolute monotonic deadline."""
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise HarnessError(f"deadline expired while waiting for {self.name}")
        try:
            return self.process.wait(timeout=remaining)
        except subprocess.TimeoutExpired as error:
            raise HarnessError(f"deadline expired while waiting for {self.name}") from error

    def _signal_session(self, requested_signal: signal.Signals) -> None:
        try:
            os.killpg(self.process.pid, requested_signal)
        except ProcessLookupError:
            pass

    def close(self, deadline: float) -> None:
        """Terminate, wait, then group-kill and wait within the deadline."""
        if self._closed:
            return
        try:
            if self.process.poll() is None:
                self._signal_session(signal.SIGTERM)
                remaining = max(0.0, deadline - time.monotonic())
                terminate_deadline = time.monotonic() + remaining / 2.0
                try:
                    self.wait_until_exit(terminate_deadline)
                except HarnessError:
                    self._signal_session(signal.SIGKILL)
                    self.wait_until_exit(deadline)
        finally:
            self._log_file.close()
            self._closed = True


class HarnessRuntime:
    """Own a private runtime root and child stack for one invocation."""

    def __init__(self, prefix: str = "fvwm-live-x-") -> None:
        self._temporary = tempfile.TemporaryDirectory(prefix=prefix)
        self.root = Path(self._temporary.name)
        self.processes: list[ManagedProcess] = []

    def environment(
        self, overrides: dict[str, str] | None = None
    ) -> dict[str, str]:
        return sanitized_environment(self.root, overrides)

    def start(
        self,
        name: str,
        argv: list[str],
        log_name: str,
        environment: dict[str, str] | None = None,
        pass_fds: tuple[int, ...] = (),
    ) -> ManagedProcess:
        process = ManagedProcess.start(
            name,
            argv,
            self.root / log_name,
            environment if environment is not None else self.environment(),
            pass_fds,
        )
        self.processes.append(process)
        return process

    def close(self, timeout_per_process: float = 2.0) -> None:
        """Tear children down in reverse launch order, each with a hard bound."""
        first_error: BaseException | None = None
        for process in reversed(self.processes):
            try:
                process.close(time.monotonic() + timeout_per_process)
            except BaseException as error:
                if first_error is None:
                    first_error = error
        self._temporary.cleanup()
        if first_error is not None:
            raise first_error

    def __enter__(self) -> "HarnessRuntime":
        return self

    def __exit__(self, error_type: object, error: object, traceback: object) -> None:
        try:
            self.close()
        except BaseException:
            if error is None:
                raise


def wait_for_predicate(
    label: str,
    predicate: object,
    deadline: float,
    readable_fds: tuple[int, ...] = (),
    process: ManagedProcess | None = None,
) -> object:
    """Poll a value-producing predicate to one monotonic hard deadline."""
    with selectors.DefaultSelector() as selector:
        for fd in readable_fds:
            selector.register(fd, selectors.EVENT_READ)
        while True:
            value = predicate()
            if value:
                return value
            if process is not None and process.poll() is not None:
                raise HarnessError(
                    f"{process.name} exited with {process.poll()} while waiting for {label}"
                )
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise HarnessError(f"deadline expired while waiting for {label}")
            selector.select(min(remaining, POLL_QUANTUM_SECONDS))


def _validate_display_number(data: bytes) -> str:
    try:
        display_number = data.decode("ascii")
    except UnicodeDecodeError as error:
        raise HarnessError("X server returned a non-ASCII display number") from error
    if not display_number or not display_number.isascii() or not display_number.isdigit():
        raise HarnessError(f"X server returned invalid display text: {display_number!r}")
    if int(display_number) > 65535:
        raise HarnessError(f"X server display number is out of range: {display_number}")
    return f":{display_number}"


def read_display_fd(
    read_fd: int, process: ManagedProcess, deadline: float
) -> str:
    """Read and validate one newline-terminated Xserver -displayfd result."""
    os.set_blocking(read_fd, False)
    buffer = bytearray()

    def read_result() -> str | None:
        try:
            chunk = os.read(read_fd, 64)
        except BlockingIOError:
            return None
        if chunk:
            buffer.extend(chunk)
        if b"\n" in buffer:
            line, extra = bytes(buffer).split(b"\n", 1)
            if extra.strip():
                raise HarnessError("X server returned extra display-fd data")
            return _validate_display_number(line)
        if not chunk:
            raise HarnessError("X server closed display-fd before reporting readiness")
        if len(buffer) > 16:
            raise HarnessError("X server display-fd response is too long")
        return None

    result = wait_for_predicate(
        "X server display-fd readiness",
        read_result,
        deadline,
        readable_fds=(read_fd,),
        process=process,
    )
    if type(result) is not str:
        raise HarnessError("X server display-fd predicate returned an invalid value")
    return result


def start_display_server(
    runtime: HarnessRuntime,
    name: str,
    argv_prefix: list[str],
    log_name: str,
    deadline: float,
    environment: dict[str, str] | None = None,
) -> tuple[ManagedProcess, str]:
    """Launch an X server with collision-safe display allocation and no TCP."""
    read_fd, write_fd = os.pipe()
    process: ManagedProcess | None = None
    try:
        if not argv_prefix:
            raise ValueError("X server argv prefix must not be empty")
        argv = [
            argv_prefix[0],
            "-displayfd",
            str(write_fd),
            "-nolisten",
            "tcp",
            *argv_prefix[1:],
        ]
        process = runtime.start(
            name,
            argv,
            log_name,
            environment=environment,
            pass_fds=(write_fd,),
        )
    finally:
        os.close(write_fd)
    try:
        if process is None:
            raise HarnessError(f"failed to start {name}")
        return process, read_display_fd(read_fd, process, deadline)
    finally:
        os.close(read_fd)


def xorg_dummy_server_argv(config_path: Path, display_fd: int) -> list[str]:
    """Return the exact private authoritative-server launch vector."""
    if display_fd < 0:
        raise ValueError("display fd must be nonnegative")
    return [
        "Xorg",
        "-config",
        str(config_path),
        "-displayfd",
        str(display_fd),
        "-nolisten",
        "tcp",
        "-noreset",
    ]


def start_xorg_dummy(
    runtime: HarnessRuntime,
    config_path: Path,
    deadline: float,
) -> tuple[ManagedProcess, str]:
    """Launch Xorg-dummy with its required exact argv ordering."""
    read_fd, write_fd = os.pipe()
    process: ManagedProcess | None = None
    try:
        process = runtime.start(
            "Xorg-dummy",
            xorg_dummy_server_argv(config_path, write_fd),
            "xserver.log",
            pass_fds=(write_fd,),
        )
    finally:
        os.close(write_fd)
    try:
        if process is None:
            raise HarnessError("failed to start Xorg-dummy")
        return process, read_display_fd(read_fd, process, deadline)
    finally:
        os.close(read_fd)


def run_command(
    runtime: HarnessRuntime,
    name: str,
    argv: list[str],
    log_name: str,
    deadline: float,
    environment: dict[str, str] | None = None,
) -> tuple[int, str]:
    """Run an owned one-shot command and return its status and combined log."""
    process = runtime.start(name, argv, log_name, environment=environment)
    try:
        return_code = process.wait_until_exit(deadline)
    finally:
        process.close(time.monotonic() + 1.0)
    return return_code, process.log_path.read_text(errors="replace")


def query_xrandr(
    runtime: HarnessRuntime,
    display: str,
    arguments: list[str],
    deadline: float,
    log_name: str = "xrandr.log",
) -> tuple[int, str]:
    """Run xrandr with validated display text and an argv-only command."""
    if not display.startswith(":") or not display[1:].isdigit():
        raise ValueError(f"invalid display: {display!r}")
    return run_command(
        runtime,
        "xrandr",
        ["xrandr", "--display", display, *arguments],
        log_name,
        deadline,
    )


def wait_for_randr(runtime: HarnessRuntime, display: str, deadline: float) -> str:
    """Wait until xrandr can query the allocated nested display."""
    attempt = 0

    def query() -> str | None:
        nonlocal attempt
        attempt += 1
        return_code, output = query_xrandr(
            runtime,
            display,
            ["--query"],
            deadline,
            f"xrandr-ready-{attempt}.log",
        )
        return output if return_code == 0 else None

    result = wait_for_predicate("RandR query readiness", query, deadline)
    if type(result) is not str:
        raise HarnessError("RandR readiness returned invalid output")
    return result


def parse_randr_version(output: str) -> tuple[int, int]:
    """Return the server RandR version reported by xrandr."""
    match = re.search(r"^Server reports RandR version (\d+)\.(\d+)\s*$", output,
                      re.MULTILINE)
    if match is None:
        raise HarnessError("xrandr did not report the server RandR version")
    return int(match.group(1)), int(match.group(2))


def parse_xrandr_monitors(output: str) -> dict[str, dict[str, object]]:
    """Parse strict xrandr --listmonitors state keyed by Monitor name."""
    lines = output.splitlines()
    if not lines:
        raise HarnessError("xrandr --listmonitors returned no output")
    count_match = re.fullmatch(r"Monitors:\s+(\d+)", lines[0].strip())
    if count_match is None:
        raise HarnessError("xrandr --listmonitors returned an invalid header")

    pattern = re.compile(
        r"^\s*\d+:\s+([+*]*)(\S+)\s+"
        r"(\d+)/(\d+)x(\d+)/(\d+)([+-]\d+)([+-]\d+)"
        r"(?:\s+(.*?))?\s*$"
    )
    monitors: dict[str, dict[str, object]] = {}
    for line in lines[1:]:
        if not line.strip():
            continue
        match = pattern.fullmatch(line)
        if match is None:
            raise HarnessError(f"invalid xrandr Monitor line: {line!r}")
        flags, name = match.group(1), match.group(2)
        if name in monitors:
            raise HarnessError(f"duplicate xrandr Monitor name: {name!r}")
        output_text = match.group(9)
        monitors[name] = {
            "name": name,
            "primary": "*" in flags,
            "width": int(match.group(3)),
            "mm_width": int(match.group(4)),
            "height": int(match.group(5)),
            "mm_height": int(match.group(6)),
            "x": int(match.group(7)),
            "y": int(match.group(8)),
            "output_names": output_text.split() if output_text else [],
        }
    if len(monitors) != int(count_match.group(1)):
        raise HarnessError("xrandr Monitor count does not match its header")
    return monitors


def parse_xrandr_output_ids(output: str) -> dict[str, int]:
    """Parse output XIDs from xrandr --verbose Identifier records."""
    output_ids: dict[str, int] = {}
    current_name: str | None = None
    for line in output.splitlines():
        output_match = re.match(
            r"^(\S+)\s+(?:connected|disconnected)\b", line
        )
        if output_match is not None:
            current_name = output_match.group(1)
            continue
        identifier_match = re.match(r"^\s+Identifier:\s+(0x[0-9a-fA-F]+)\s*$", line)
        if identifier_match is None:
            continue
        if current_name is None or current_name in output_ids:
            raise HarnessError("xrandr --verbose returned an invalid Identifier")
        output_ids[current_name] = int(identifier_match.group(1), 16)
    if not output_ids:
        raise HarnessError("xrandr --verbose returned no output Identifiers")
    return output_ids


def parse_topology_policy_constants(header_text: str) -> tuple[int, int]:
    """Read live timing bounds from the production policy header text."""
    values: list[int] = []
    for name in ("FVWM_TOPOLOGY_QUIET_MS", "FVWM_TOPOLOGY_MAX_LATENCY_MS"):
        match = re.search(rf"^#define\s+{name}\s+(\d+)\s*$", header_text,
                          re.MULTILINE)
        if match is None:
            raise HarnessError(f"policy header does not define {name}")
        values.append(int(match.group(1)))
    quiet_ms, maximum_ms = values
    if quiet_ms <= 0 or maximum_ms < quiet_ms:
        raise HarnessError("topology policy timing constants are invalid")
    return quiet_ms, maximum_ms


def parse_xrandr_root_dimensions(output: str) -> dict[str, int]:
    """Parse framebuffer pixels and the aggregate connected-output millimetres."""
    screen = re.search(
        r"^Screen \d+: .* current (\d+) x (\d+),", output, re.MULTILINE
    )
    if screen is None:
        raise HarnessError("xrandr query did not report current screen dimensions")
    output_sizes = [
        (int(match.group(1)), int(match.group(2)))
        for match in re.finditer(
            r"^\S+ connected(?: primary)? .*? (\d+)mm x (\d+)mm\s*$",
            output,
            re.MULTILINE,
        )
    ]
    if not output_sizes:
        raise HarnessError("xrandr query did not report connected output sizes")
    return {
        "width": int(screen.group(1)),
        "height": int(screen.group(2)),
        "mm_width": sum(width for width, _ in output_sizes),
        "mm_height": max(height for _, height in output_sizes),
    }


def query_xlib_root_dimensions(display: str) -> dict[str, int]:
    """Query authoritative root pixel and physical dimensions through Xlib."""
    library_name = ctypes.util.find_library("X11")
    if library_name is None:
        raise HarnessError("libX11 is unavailable for root-dimension queries")
    xlib = ctypes.CDLL(library_name)
    xlib.XOpenDisplay.argtypes = [ctypes.c_char_p]
    xlib.XOpenDisplay.restype = ctypes.c_void_p
    xlib.XDefaultScreen.argtypes = [ctypes.c_void_p]
    xlib.XDefaultScreen.restype = ctypes.c_int
    for name in ("XDisplayWidth", "XDisplayHeight", "XDisplayWidthMM",
                 "XDisplayHeightMM"):
        function = getattr(xlib, name)
        function.argtypes = [ctypes.c_void_p, ctypes.c_int]
        function.restype = ctypes.c_int
    xlib.XCloseDisplay.argtypes = [ctypes.c_void_p]
    xlib.XCloseDisplay.restype = ctypes.c_int

    handle = xlib.XOpenDisplay(display.encode("ascii"))
    if not handle:
        raise HarnessError(f"XOpenDisplay failed for {display}")
    try:
        screen = xlib.XDefaultScreen(handle)
        return {
            "width": xlib.XDisplayWidth(handle, screen),
            "height": xlib.XDisplayHeight(handle, screen),
            "mm_width": xlib.XDisplayWidthMM(handle, screen),
            "mm_height": xlib.XDisplayHeightMM(handle, screen),
        }
    finally:
        xlib.XCloseDisplay(handle)


def start_fvwm(
    runtime: HarnessRuntime,
    fvwm_path: Path,
    display: str,
    deadline: float,
    environment: dict[str, str] | None = None,
) -> tuple[ManagedProcess, "StatusCollector"]:
    """Start FVWM with Status enabled and wait for its private FIFO."""
    if not fvwm_path.is_file():
        raise ValueError(f"FVWM executable does not exist: {fvwm_path}")
    fvwm_log = runtime.root / "fvwm.log"
    process = runtime.start(
        "fvwm3",
        fvwm_arguments(fvwm_path, display, fvwm_log),
        "fvwm-process.log",
        environment=environment,
    )
    collector = StatusCollector.open(
        runtime.root / "fvwm3.pipe", deadline, process=process
    )
    return process, collector


def fvwm_arguments(
    fvwm_path: Path,
    display: str,
    fvwm_log: Path,
    extra_commands: tuple[str, ...] = (),
) -> list[str]:
    """Build the fixed FVWM startup argv used by live-X probes."""
    arguments = [
        str(fvwm_path),
        "-d",
        display,
        "-o",
        str(fvwm_log),
        "-v",
        "-c",
        "Schedule 0 Status On",
        "-c",
        "PrintInfo Topology",
    ]
    for command in extra_commands:
        if type(command) is not str or "\0" in command:
            raise ValueError("FVWM startup commands must be NUL-free strings")
        arguments.extend(("-c", command))
    return arguments


def add_post_finalize_scale_inspection(arguments: list[str]) -> None:
    """Append one scheduled inspection after all initial config directives."""
    arguments.extend(("-c", "Schedule 0 PrintInfo Scale"))


_SCALE_FACTORS = {
    "1", "1.25", "1.5", "1.75", "2", "2.25", "2.5", "2.75",
    "3", "3.25", "3.5", "3.75", "4",
}
_SCALE_SOURCES = {"override", "automatic", "fallback", "safe"}
_SCALE_MATCH_STATUSES = {"none", "unique", "ambiguous"}
_SCALE_AUTO_STATUSES = {
    "valid", "missing-axis", "x-implausible", "y-implausible", "inconsistent",
}
_SCALE_BOUNDARIES = {"none", "low", "high"}
_SCALE_REASONS = {
    "exact-override", "valid-automatic", "automatic-bound-low",
    "automatic-bound-high", "configured-fallback", "safe-default",
}


def _parse_scale_name(line: str, prefix: str) -> tuple[str, str]:
    if not line.startswith(prefix):
        raise HarnessError("PrintInfo Scale line has an invalid prefix")
    decoder = json.JSONDecoder()
    try:
        name, end = decoder.raw_decode(line, len(prefix))
    except json.JSONDecodeError as error:
        raise HarnessError("PrintInfo Scale name is not valid JSON") from error
    if type(name) is not str:
        raise HarnessError("PrintInfo Scale name must decode to a string")
    return name, line[end:]


def parse_print_info_scale_line(line: str) -> dict[str, object]:
    """Parse one complete reason-bearing PrintInfo Scale evidence line."""
    if line.startswith("Scale monitor name="):
        name, remainder = _parse_scale_name(line, "Scale monitor name=")
        match = re.fullmatch(
            r" factor=(\S+) source=(\S+) pixels=(\d+)x(\d+) "
            r"physical_mm=(\d+)x(\d+) match_status=(\S+) "
            r"auto_status=(\S+) boundary=(\S+) reason=(\S+)",
            remainder,
        )
        if match is None:
            raise HarnessError("PrintInfo Scale monitor fields are malformed")
        (factor, source, width, height, mm_width, mm_height, match_status,
         auto_status, boundary, reason) = match.groups()
        if (
            factor not in _SCALE_FACTORS
            or source not in _SCALE_SOURCES
            or match_status not in _SCALE_MATCH_STATUSES
            or auto_status not in _SCALE_AUTO_STATUSES
            or boundary not in _SCALE_BOUNDARIES
            or reason not in _SCALE_REASONS
        ):
            raise HarnessError("PrintInfo Scale monitor field value is invalid")
        return {
            "kind": "monitor",
            "name": name,
            "factor": factor,
            "source": source,
            "pixels": (int(width), int(height)),
            "physical_mm": (int(mm_width), int(mm_height)),
            "match_status": match_status,
            "auto_status": auto_status,
            "boundary": boundary,
            "reason": reason,
        }
    if line.startswith("Scale override name="):
        name, remainder = _parse_scale_name(line, "Scale override name=")
        match = re.fullmatch(
            r" factor=(\S+) match_status=(\S+) state=(\S+)", remainder
        )
        if match is None:
            raise HarnessError("PrintInfo Scale override fields are malformed")
        factor, match_status, state = match.groups()
        if (
            factor not in _SCALE_FACTORS
            or match_status != "none"
            or state != "retained-unmatched"
        ):
            raise HarnessError("PrintInfo Scale override field value is invalid")
        return {
            "kind": "override",
            "name": name,
            "factor": factor,
            "match_status": match_status,
            "state": state,
        }
    raise HarnessError("line is not PrintInfo Scale evidence")


def wait_for_print_info_scale(
    log_path: Path,
    name: str,
    deadline: float,
    process: ManagedProcess,
    *,
    kind: str = "monitor",
    occurrence: int = 2,
) -> dict[str, object]:
    """Wait for the scheduled post-finalization scale record for one name."""
    if occurrence < 1:
        raise ValueError("scale evidence occurrence must be positive")

    def inspect() -> dict[str, object] | None:
        if not log_path.is_file():
            return None
        matches: list[dict[str, object]] = []
        for physical_line in log_path.read_text(errors="replace").splitlines():
            positions = [
                position
                for marker in ("Scale monitor name=", "Scale override name=")
                if (position := physical_line.find(marker)) >= 0
            ]
            if not positions:
                continue
            record = parse_print_info_scale_line(physical_line[min(positions):])
            if record["kind"] == kind and record["name"] == name:
                matches.append(record)
        return matches[occurrence - 1] if len(matches) >= occurrence else None

    result = wait_for_predicate(
        f"scheduled PrintInfo Scale {kind} {json.dumps(name)}",
        inspect,
        deadline,
        process=process,
    )
    if type(result) is not dict:
        raise HarnessError("PrintInfo Scale predicate returned invalid evidence")
    return result


def assert_print_info_scale(
    record: dict[str, object],
    *,
    name: str,
    factor: str,
    source: str,
    reason: str,
    pixels: tuple[int, int],
    physical_mm: tuple[int, int],
    match_status: str = "none",
    auto_status: str = "valid",
    boundary: str = "none",
) -> None:
    """Require one parsed active scale record to match all policy evidence."""
    expected = {
        "kind": "monitor",
        "name": name,
        "factor": factor,
        "source": source,
        "pixels": pixels,
        "physical_mm": physical_mm,
        "match_status": match_status,
        "auto_status": auto_status,
        "boundary": boundary,
        "reason": reason,
    }
    if record != expected:
        raise HarnessError(
            f"PrintInfo Scale mismatch: expected={expected!r}, actual={record!r}"
        )


def assert_scale_diagnostic(
    log_path: Path,
    fragment: str,
    deadline: float,
    process: ManagedProcess,
) -> None:
    """Wait boundedly for one exact diagnostic fragment in retained FVWM logs."""
    def contains_fragment() -> bool:
        return log_path.is_file() and fragment in log_path.read_text(errors="replace")

    wait_for_predicate(
        f"scale diagnostic {fragment!r}",
        contains_fragment,
        deadline,
        process=process,
    )


def fault_probe_metadata(mechanism: str) -> dict[str, str]:
    """Return per-invocation metadata for exactly one proven mechanism."""
    if mechanism not in ("preload", "link-wrap"):
        raise ValueError(f"unknown fault mechanism: {mechanism}")
    return {"mechanism": mechanism}


def _combined_fvwm_log(process: ManagedProcess, fvwm_log: Path) -> str:
    output = process.log_path.read_text(errors="replace")
    if fvwm_log.is_file():
        output += fvwm_log.read_text(errors="replace")
    return output


def _attempt_fault_mechanism(
    runtime: HarnessRuntime,
    *,
    mechanism: str,
    executable: Path,
    display: str,
    fault_library: Path,
    deadline: float,
    artifact_dir: Path,
) -> bool:
    """Prove one failed real query from exit, diagnostic, and sentinel claim."""
    sentinel = runtime.root / f"{mechanism}.sentinel"
    sentinel.touch(mode=0o600, exist_ok=False)
    fvwm_log = runtime.root / f"fvwm-{mechanism}.log"
    environment_overrides = {FAULT_ENV: str(sentinel)}
    if mechanism == "preload":
        environment_overrides[PRELOAD_ENV] = str(fault_library)
    environment = runtime.environment(environment_overrides)
    process = runtime.start(
        f"fvwm3-{mechanism}",
        fvwm_arguments(executable, display, fvwm_log),
        f"fvwm-{mechanism}-process.log",
        environment=environment,
    )
    return_code: int | None = None
    try:
        return_code = process.wait_until_exit(deadline)
    except HarnessError:
        pass
    finally:
        process.close(time.monotonic() + 2.0)

    output = _combined_fvwm_log(process, fvwm_log)
    (artifact_dir / f"{mechanism}.log").write_text(output)
    diagnostic = "RandR monitor query failed; retaining last topology"
    observed = (
        return_code == 101
        and not sentinel.exists()
        and output.count(diagnostic) == 1
        and output.count("Unable to initialise RandR") >= 1
    )
    attempt = {
        "mechanism": mechanism,
        "observed": observed,
        "returncode": return_code,
        "sentinel_consumed": not sentinel.exists(),
        "query_failure_diagnostics": output.count(diagnostic),
    }
    (artifact_dir / f"{mechanism}.json").write_text(
        json.dumps(attempt, indent=2, sort_keys=True) + "\n"
    )
    return observed


def _prove_ordinary_production_startup(
    runtime: HarnessRuntime,
    fvwm_path: Path,
    display: str,
    deadline: float,
) -> None:
    """Prove the production target starts without either fault control."""
    environment = runtime.environment()
    if FAULT_ENV in environment or PRELOAD_ENV in environment:
        raise HarnessError("ordinary FVWM environment contains fault controls")
    if b"__wrap_XRRGetMonitors" in fvwm_path.read_bytes():
        raise HarnessError("production FVWM contains the test-only wrapper symbol")

    process, collector = start_fvwm(
        runtime, fvwm_path, display, deadline, environment=environment
    )
    try:
        if process.poll() is not None:
            raise HarnessError(
                f"ordinary production FVWM exited with {process.poll()}"
            )
    finally:
        collector.close()
        process.close(time.monotonic() + 2.0)


def run_fault_injection_probe(options: argparse.Namespace) -> None:
    """Probe preload first, then prove linker wrapping only as fallback."""
    required_paths = {
        "--fvwm": options.fvwm,
        "--wrapped-fvwm": options.wrapped_fvwm,
        "--fault-library": options.fault_library,
    }
    for option_name, path in required_paths.items():
        if path is None or not path.is_file():
            raise ValueError(f"{option_name} must name an existing file")
    if options.artifacts_dir is None:
        raise ValueError("--artifacts-dir is required")
    if options.backend != "xvfb":
        raise ValueError("fault-injection-probe requires --backend=xvfb")

    fvwm_path = options.fvwm.resolve()
    wrapped_fvwm = options.wrapped_fvwm.resolve()
    fault_library = options.fault_library.resolve()
    artifact_dir = options.artifacts_dir.resolve()
    artifact_dir.mkdir(parents=True, exist_ok=True)

    runtime = HarnessRuntime(prefix="fvwm-fault-probe-")
    server: ManagedProcess | None = None
    xrandr_text: str | None = None
    last_status: dict[str, object] | None = None
    try:
        server, display = start_display_server(
            runtime,
            "Xvfb",
            ["Xvfb", "-screen", "0", "1024x768x24"],
            "xserver.log",
            time.monotonic() + 10.0,
        )
        xrandr_text = wait_for_randr(runtime, display, time.monotonic() + 10.0)

        if _attempt_fault_mechanism(
            runtime,
            mechanism="preload",
            executable=fvwm_path,
            display=display,
            fault_library=fault_library,
            deadline=time.monotonic() + 10.0,
            artifact_dir=artifact_dir,
        ):
            mechanism = "preload"
        elif _attempt_fault_mechanism(
            runtime,
            mechanism="link-wrap",
            executable=wrapped_fvwm,
            display=display,
            fault_library=fault_library,
            deadline=time.monotonic() + 10.0,
            artifact_dir=artifact_dir,
        ):
            mechanism = "link-wrap"
        else:
            raise HarnessError("neither XRR fault mechanism affected actual FVWM")

        _prove_ordinary_production_startup(
            runtime, fvwm_path, display, time.monotonic() + 10.0
        )
        metadata = fault_probe_metadata(mechanism)
        (artifact_dir / "probe-result.json").write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + "\n"
        )
    except BaseException as error:
        capture_failure(
            artifact_dir,
            error=error,
            server_log=server.log_path if server is not None else None,
            fvwm_log=runtime.root / "fvwm.log",
            xrandr_text=xrandr_text,
            last_status=last_status,
            metadata={"backend": "xvfb", "scenario": "fault-injection-probe"},
        )
        raise
    finally:
        runtime.close()


class StatusCollector:
    """Collect strict newline-delimited Status JSON from a nonblocking FIFO."""

    def __init__(self, path: Path, file_descriptor: int) -> None:
        self.path = path
        self.file_descriptor = file_descriptor
        self._buffer = bytearray()
        self.records: list[dict[str, object]] = []
        self.last_status: dict[str, object] | None = None

    @classmethod
    def open(
        cls,
        path: Path,
        deadline: float,
        process: ManagedProcess | None = None,
    ) -> "StatusCollector":
        wait_for_predicate(
            "Status FIFO creation", path.is_fifo, deadline, process=process
        )
        file_descriptor = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
        return cls(path, file_descriptor)

    def close(self) -> None:
        if self.file_descriptor >= 0:
            os.close(self.file_descriptor)
            self.file_descriptor = -1

    def _parse_lines(self) -> None:
        while b"\n" in self._buffer:
            line, remainder = self._buffer.split(b"\n", 1)
            self._buffer = bytearray(remainder)
            if not line:
                continue
            if len(line) > MAX_STATUS_LINE_BYTES:
                raise HarnessError("Status JSON line exceeds harness limit")
            try:
                document = json.loads(line)
            except (UnicodeDecodeError, json.JSONDecodeError) as error:
                raise HarnessError("Status emitted invalid JSON") from error
            if type(document) is not dict:
                raise HarnessError("Status JSON must be an object")
            self.records.append(document)
            self.last_status = document

    def read_available(self) -> None:
        while True:
            try:
                chunk = os.read(self.file_descriptor, 65536)
            except BlockingIOError:
                break
            if not chunk:
                break
            self._buffer.extend(chunk)
            if len(self._buffer) > MAX_STATUS_LINE_BYTES:
                raise HarnessError("unterminated Status JSON exceeds harness limit")
        self._parse_lines()

    def wait_for(
        self,
        label: str,
        predicate: object,
        deadline: float,
        process: ManagedProcess | None = None,
    ) -> dict[str, object]:
        def inspect() -> dict[str, object] | None:
            self.read_available()
            for record in reversed(self.records):
                if predicate(record):
                    return record
            return None

        result = wait_for_predicate(
            label,
            inspect,
            deadline,
            readable_fds=(self.file_descriptor,),
            process=process,
        )
        if type(result) is not dict:
            raise HarnessError("Status predicate returned an invalid record")
        return result

    def __enter__(self) -> "StatusCollector":
        return self

    def __exit__(self, error_type: object, error: object, traceback: object) -> None:
        self.close()


def status_topology(status: dict[str, object]) -> dict[str, object]:
    """Return a strictly typed Status v4 topology object."""
    if status.get("version") != 4 or type(status.get("version")) is not int:
        raise HarnessError("expected Status version 4")
    topology = status.get("topology")
    if type(topology) is not dict:
        raise HarnessError("Status topology must be an object")
    epoch = topology.get("epoch")
    monitors = topology.get("monitors")
    if type(epoch) is not int or epoch < 1:
        raise HarnessError("Status topology epoch must be a positive integer")
    if type(monitors) is not list:
        raise HarnessError("Status topology monitors must be an array")
    return topology


def topology_epoch(status: dict[str, object]) -> int:
    epoch = status_topology(status)["epoch"]
    if type(epoch) is not int:
        raise HarnessError("Status topology epoch has an invalid type")
    return epoch


def assert_topology_retained(
    known_good: dict[str, object], retained: dict[str, object]
) -> None:
    """Require byte-canonical and semantic equality of last-good topology."""
    known_topology = status_topology(known_good)
    retained_topology = status_topology(retained)
    known_bytes = json.dumps(
        known_topology, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    retained_bytes = json.dumps(
        retained_topology, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    if retained_topology != known_topology or retained_bytes != known_bytes:
        raise HarnessError("failed query changed last-good topology or epoch")


def topology_monitor(
    status: dict[str, object], name: str
) -> dict[str, object]:
    monitors = status_topology(status)["monitors"]
    if type(monitors) is not list:
        raise HarnessError("Status topology monitor collection has an invalid type")
    matches = [
        monitor
        for monitor in monitors
        if type(monitor) is dict and monitor.get("name") == name
    ]
    if len(matches) != 1:
        raise HarnessError(f"expected exactly one topology monitor named {name!r}")
    return matches[0]


def assert_status_monitor_matches(
    status: dict[str, object],
    queried_monitor: dict[str, object],
    expected_outputs: list[int],
    *,
    expected_epoch: int,
) -> None:
    """Require exact Status v4 facts for one queried RandR Monitor."""
    topology = status_topology(status)
    if topology["epoch"] != expected_epoch:
        raise HarnessError(
            f"expected topology epoch {expected_epoch}, got {topology['epoch']}"
        )
    name = queried_monitor.get("name")
    if type(name) is not str:
        raise HarnessError("queried Monitor has an invalid name")
    monitor = topology_monitor(status, name)
    required_keys = {
        "name",
        "name_atom",
        "primary",
        "geometry",
        "physical_mm",
        "outputs",
    }
    if set(monitor) != required_keys:
        raise HarnessError(f"Status monitor {name!r} has unexpected keys")
    if type(monitor["name_atom"]) is not int or monitor["name_atom"] <= 0:
        raise HarnessError(f"Status monitor {name!r} has an invalid name Atom")
    if type(monitor["primary"]) is not bool:
        raise HarnessError(f"Status monitor {name!r} primary is not boolean")
    if monitor["primary"] != queried_monitor.get("primary"):
        raise HarnessError(f"Status monitor {name!r} primary does not match RandR")

    geometry = monitor["geometry"]
    physical = monitor["physical_mm"]
    if type(geometry) is not dict or set(geometry) != {
        "x",
        "y",
        "width",
        "height",
    }:
        raise HarnessError(f"Status monitor {name!r} geometry is invalid")
    if type(physical) is not dict or set(physical) != {"width", "height"}:
        raise HarnessError(f"Status monitor {name!r} physical size is invalid")
    expected_geometry = {
        key: queried_monitor.get(key) for key in ("x", "y", "width", "height")
    }
    expected_physical = {
        "width": queried_monitor.get("mm_width"),
        "height": queried_monitor.get("mm_height"),
    }
    if any(type(value) is not int for value in geometry.values()):
        raise HarnessError(f"Status monitor {name!r} geometry is not integral")
    if any(type(value) is not int for value in physical.values()):
        raise HarnessError(f"Status monitor {name!r} physical size is not integral")
    if geometry != expected_geometry or physical != expected_physical:
        raise HarnessError(f"Status monitor {name!r} dimensions do not match RandR")
    outputs = monitor["outputs"]
    if type(outputs) is not list or any(type(value) is not int for value in outputs):
        raise HarnessError(f"Status monitor {name!r} outputs are invalid")
    if outputs != expected_outputs:
        raise HarnessError(f"Status monitor {name!r} outputs do not match RandR")


def assert_status_stable(
    collector: StatusCollector,
    expected_status: dict[str, object],
    deadline: float,
    process: ManagedProcess,
) -> None:
    """Observe through a predicate deadline without accepting Status changes."""
    with selectors.DefaultSelector() as selector:
        selector.register(collector.file_descriptor, selectors.EVENT_READ)
        while True:
            collector.read_available()
            for record in collector.records:
                if record != expected_status:
                    raise HarnessError("Status changed without queried topology change")
            if process.poll() is not None:
                raise HarnessError(
                    f"{process.name} exited with {process.poll()} during stability check"
                )
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return
            selector.select(min(remaining, POLL_QUANTUM_SECONDS))


def assert_status_matches_monitors(
    status: dict[str, object],
    queried_monitors: dict[str, dict[str, object]],
    output_ids: dict[str, int],
    *,
    expected_epoch: int,
) -> None:
    """Require one exact Status Monitor for every queried RandR Monitor."""
    topology = status_topology(status)
    status_monitors = topology["monitors"]
    if type(status_monitors) is not list:
        raise HarnessError("Status topology monitor collection has an invalid type")
    if len(status_monitors) != len(queried_monitors):
        raise HarnessError("Status and queried RandR Monitor counts differ")
    for monitor in queried_monitors.values():
        output_names = monitor.get("output_names")
        if type(output_names) is not list or any(
            type(name) is not str for name in output_names
        ):
            raise HarnessError("queried RandR Monitor membership is invalid")
        try:
            expected_outputs = sorted({output_ids[name] for name in output_names})
        except KeyError as error:
            raise HarnessError(
                f"queried output {error.args[0]!r} has no XID"
            ) from error
        assert_status_monitor_matches(
            status,
            monitor,
            expected_outputs,
            expected_epoch=expected_epoch,
        )


def poll_xrandr_monitors(
    runtime: HarnessRuntime,
    display: str,
    label: str,
    predicate: object,
    deadline: float,
    log_prefix: str,
) -> tuple[dict[str, dict[str, object]], str] | None:
    """Poll parsed Monitor state; command status never proves capability."""
    attempt = 0
    successful_queries = 0
    with selectors.DefaultSelector() as selector:
        while True:
            attempt += 1
            _, output = query_xrandr(
                runtime,
                display,
                ["--listmonitors"],
                deadline,
                f"{log_prefix}-{attempt}.log",
            )
            try:
                monitors = parse_xrandr_monitors(output)
            except HarnessError:
                monitors = None
            if monitors is not None:
                successful_queries += 1
                if predicate(monitors):
                    return monitors, output
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                if successful_queries == 0:
                    raise HarnessError(
                        f"no parseable Monitor state while waiting for {label}"
                    )
                return None
            selector.select(min(remaining, POLL_QUANTUM_SECONDS))


def wait_for_status_monitors(
    collector: StatusCollector,
    process: ManagedProcess,
    queried_monitors: dict[str, dict[str, object]],
    output_ids: dict[str, int],
    expected_epoch: int,
    deadline: float,
) -> dict[str, object]:
    """Poll Status until it exactly matches one queried Monitor state."""
    def matches(record: dict[str, object]) -> bool:
        try:
            assert_status_matches_monitors(
                record,
                queried_monitors,
                output_ids,
                expected_epoch=expected_epoch,
            )
        except HarnessError:
            return False
        return True

    return collector.wait_for(
        f"Status topology epoch {expected_epoch}",
        matches,
        deadline,
        process=process,
    )


def query_authoritative_state(
    runtime: HarnessRuntime,
    display: str,
    deadline: float,
    log_prefix: str,
) -> dict[str, object]:
    """Query complete Monitor, output-XID, and root-dimension authority."""
    outputs: dict[str, str] = {}
    for name, arguments in (
        ("query", ["--query"]),
        ("monitors", ["--listmonitors"]),
        ("verbose", ["--verbose"]),
    ):
        return_code, text = query_xrandr(
            runtime,
            display,
            arguments,
            deadline,
            f"{log_prefix}-{name}.log",
        )
        if return_code != 0:
            raise HarnessError(f"xrandr {name} query failed")
        outputs[name] = text
    root_dimensions = query_xlib_root_dimensions(display)
    parsed_query = parse_xrandr_root_dimensions(outputs["query"])
    if any(root_dimensions[key] != parsed_query[key]
           for key in ("width", "height")):
        raise HarnessError("Xlib and xrandr root pixel dimensions differ")
    return {
        "monitors": parse_xrandr_monitors(outputs["monitors"]),
        "output_ids": parse_xrandr_output_ids(outputs["verbose"]),
        "root": root_dimensions,
        "text": "\n".join(outputs.values()),
    }


def wait_for_authoritative_state(
    runtime: HarnessRuntime,
    display: str,
    label: str,
    predicate: object,
    deadline: float,
    log_prefix: str,
) -> dict[str, object]:
    """Poll the complete queried server state to one hard deadline."""
    attempt = 0

    def query() -> dict[str, object] | None:
        nonlocal attempt
        attempt += 1
        try:
            state = query_authoritative_state(
                runtime, display, deadline, f"{log_prefix}-{attempt}"
            )
        except HarnessError:
            return None
        return state if predicate(state) else None

    result = wait_for_predicate(label, query, deadline)
    if type(result) is not dict:
        raise HarnessError("authoritative-state predicate returned invalid data")
    return result


def state_monitors(state: dict[str, object]) -> dict[str, dict[str, object]]:
    monitors = state.get("monitors")
    if type(monitors) is not dict:
        raise HarnessError("queried state has invalid Monitor data")
    return monitors


def state_output_ids(state: dict[str, object]) -> dict[str, int]:
    output_ids = state.get("output_ids")
    if type(output_ids) is not dict or any(
        type(name) is not str or type(value) is not int
        for name, value in output_ids.items()
    ):
        raise HarnessError("queried state has invalid output XIDs")
    return output_ids


def expected_monitor(
    name: str,
    *,
    primary: bool,
    width: int,
    height: int,
    x: int,
    y: int,
    mm_width: int,
    mm_height: int,
    output_names: list[str],
) -> dict[str, object]:
    return {
        "name": name,
        "primary": primary,
        "width": width,
        "height": height,
        "x": x,
        "y": y,
        "mm_width": mm_width,
        "mm_height": mm_height,
        "output_names": output_names,
    }


def collector_position(collector: StatusCollector) -> int:
    collector.read_available()
    return len(collector.records)


def assert_no_status_after(
    collector: StatusCollector,
    position: int,
    deadline: float,
    process: ManagedProcess,
    label: str,
) -> None:
    """Prove no downstream Status emission through a predicate deadline."""
    with selectors.DefaultSelector() as selector:
        selector.register(collector.file_descriptor, selectors.EVENT_READ)
        while True:
            collector.read_available()
            if len(collector.records) != position:
                raise HarnessError(f"unexpected Status emission during {label}")
            if process.poll() is not None:
                raise HarnessError(f"FVWM exited during {label}")
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return
            selector.select(min(remaining, POLL_QUANTUM_SECONDS))


def assert_print_info_topology(
    log_path: Path,
    status: dict[str, object],
    deadline: float,
    process: ManagedProcess,
) -> None:
    """Require PrintInfo text to describe the same epoch and monitor facts."""
    topology = status_topology(status)
    monitors = topology["monitors"]
    if type(monitors) is not list:
        raise HarnessError("Status monitor array is invalid")

    def matches() -> bool:
        if not log_path.is_file():
            return False
        text = log_path.read_text(errors="replace")
        if f"Topology epoch={topology['epoch']} monitors={len(monitors)}" not in text:
            return False
        for monitor in monitors:
            if type(monitor) is not dict:
                return False
            geometry = monitor.get("geometry")
            physical = monitor.get("physical_mm")
            outputs = monitor.get("outputs")
            if type(geometry) is not dict or type(physical) is not dict:
                return False
            if type(outputs) is not list:
                return False
            fragment = (
                f"Topology monitor name={json.dumps(monitor.get('name'))} "
                f"name_atom={monitor.get('name_atom')} "
                f"primary={str(monitor.get('primary')).lower()} "
                f"geometry={geometry.get('width')}x{geometry.get('height')}"
                f"{geometry.get('x'):+d}{geometry.get('y'):+d} "
                f"physical_mm={physical.get('width')}x{physical.get('height')} "
                f"outputs=[{','.join(str(value) for value in outputs)}]"
            )
            if fragment not in text:
                return False
        return True

    wait_for_predicate(
        "PrintInfo exact topology facts", matches, deadline, process=process
    )


def run_required_xrandr(
    runtime: HarnessRuntime,
    display: str,
    arguments: list[str],
    operation: str,
    operations: list[dict[str, object]],
    *,
    maximum_seconds: float = 5.0,
) -> tuple[float, float]:
    """Run one required mutation and retain argv, result, and monotonic timing."""
    started = time.monotonic()
    return_code, output = query_xrandr(
        runtime,
        display,
        arguments,
        started + maximum_seconds,
        f"xrandr-{len(operations):03d}-{operation}.log",
    )
    completed = time.monotonic()
    operations.append(
        {
            "argv": ["xrandr", "--display", display, *arguments],
            "completed": completed,
            "duration_ms": (completed - started) * 1000.0,
            "operation": operation,
            "output": output,
            "returncode": return_code,
            "started": started,
        }
    )
    if return_code != 0:
        raise HarnessError(f"required xrandr operation failed: {operation}")
    if completed - started >= maximum_seconds:
        raise HarnessError(f"xrandr operation exceeded deadline: {operation}")
    return started, completed


def run_xorg_dummy_topology(options: argparse.Namespace) -> None:
    """Prove authoritative facts, canonical equality, and bounded settling."""
    required_paths = {
        "--fvwm": options.fvwm,
        "--xorg-config": options.xorg_config,
        "--policy-header": options.policy_header,
    }
    for option_name, path in required_paths.items():
        if path is None or not path.is_file():
            raise ValueError(f"{option_name} must name an existing file")
    if options.artifacts_dir is None:
        raise ValueError("--artifacts-dir is required")
    if options.backend != "xorg-dummy":
        raise ValueError("authoritative topology requires --backend=xorg-dummy")

    fvwm_path = options.fvwm.resolve()
    config_path = options.xorg_config.resolve()
    policy_header = options.policy_header.resolve()
    artifact_dir = options.artifacts_dir.resolve()
    artifact_dir.mkdir(parents=True, exist_ok=True)
    for artifact_name in (
        "failure.json",
        "fvwm.log",
        "last-status.json",
        "result.json",
        "scenario.json",
        "server.log",
        "xrandr.txt",
    ):
        (artifact_dir / artifact_name).unlink(missing_ok=True)

    quiet_ms, maximum_ms = parse_topology_policy_constants(
        policy_header.read_text()
    )
    quiet_seconds = quiet_ms / 1000.0
    runtime = HarnessRuntime(prefix="fvwm-xorg-dummy-topology-")
    server: ManagedProcess | None = None
    fvwm_process: ManagedProcess | None = None
    collector: StatusCollector | None = None
    xrandr_text: str | None = None
    last_status: dict[str, object] | None = None
    active_error: BaseException | None = None
    operations: list[dict[str, object]] = []
    scenario_metadata: dict[str, object] = {
        "backend": "xorg-dummy",
        "maximum_latency_ms": maximum_ms,
        "quiet_ms": quiet_ms,
        "scenario": "topology",
    }
    try:
        server, display = start_xorg_dummy(
            runtime, config_path, time.monotonic() + 10.0
        )
        expected_argv = xorg_dummy_server_argv(
            config_path, int(server.argv[server.argv.index("-displayfd") + 1])
        )
        if server.argv != expected_argv:
            raise HarnessError(f"unexpected Xorg-dummy argv: {server.argv!r}")
        wait_for_randr(runtime, display, time.monotonic() + 10.0)

        def mutate(arguments: list[str], operation: str,
                   maximum_seconds: float = 5.0) -> tuple[float, float]:
            return run_required_xrandr(
                runtime,
                display,
                arguments,
                operation,
                operations,
                maximum_seconds=maximum_seconds,
            )

        def settle(
            label: str,
            expected_epoch: int,
            predicate: object,
            timeout: float = 3.0,
        ) -> tuple[dict[str, object], dict[str, object]]:
            nonlocal last_status, xrandr_text
            state = wait_for_authoritative_state(
                runtime,
                display,
                label,
                predicate,
                time.monotonic() + timeout,
                f"xrandr-state-{len(operations):03d}",
            )
            xrandr_text = state["text"] if type(state.get("text")) is str else None
            if collector is None or fvwm_process is None:
                raise HarnessError("FVWM is not ready for Status comparison")
            last_status = wait_for_status_monitors(
                collector,
                fvwm_process,
                state_monitors(state),
                state_output_ids(state),
                expected_epoch,
                time.monotonic() + timeout,
            )
            return state, last_status

        mutate(
            [
                "--addmode", "DUMMY1", "1280x1024",
                "--addmode", "DUMMY1", "1024x768",
            ],
            "assign-dummy1-modes",
        )
        baseline_arguments = [
            "--output", "DUMMY0", "--mode", "1920x1080", "--pos", "0x0",
            "--primary", "--output", "DUMMY1", "--mode", "1280x1024",
            "--pos", "1920x0", "--fb", "3200x1080", "--fbmm", "846x286",
        ]
        mutate(baseline_arguments, "baseline")

        def baseline_matches(state: dict[str, object]) -> bool:
            monitors = state_monitors(state)
            root = state.get("root")
            return (
                type(root) is dict
                and root == {
                    "width": 3200,
                    "height": 1080,
                    "mm_width": 846,
                    "mm_height": 286,
                }
                and set(monitors) == {"DUMMY0", "DUMMY1"}
                and all(
                    monitors[name][key] == value
                    for name, expected in (
                        (
                            "DUMMY0",
                            expected_monitor(
                                "DUMMY0", primary=True, width=1920,
                                height=1080, x=0, y=0,
                                mm_width=monitors["DUMMY0"]["mm_width"],
                                mm_height=monitors["DUMMY0"]["mm_height"],
                                output_names=["DUMMY0"],
                            ),
                        ),
                        (
                            "DUMMY1",
                            expected_monitor(
                                "DUMMY1", primary=False, width=1280,
                                height=1024, x=1920, y=0,
                                mm_width=monitors["DUMMY1"]["mm_width"],
                                mm_height=monitors["DUMMY1"]["mm_height"],
                                output_names=["DUMMY1"],
                            ),
                        ),
                    )
                    for key, value in expected.items()
                )
            )

        baseline_state = wait_for_authoritative_state(
            runtime,
            display,
            "exact Xorg-dummy baseline",
            baseline_matches,
            time.monotonic() + 5.0,
            "xrandr-baseline",
        )
        output_ids = state_output_ids(baseline_state)
        if not {"DUMMY0", "DUMMY1"}.issubset(output_ids):
            raise HarnessError("Xorg-dummy did not expose required output XIDs")
        baseline_monitors = state_monitors(baseline_state)
        baseline_mm = {
            name: (monitor["mm_width"], monitor["mm_height"])
            for name, monitor in baseline_monitors.items()
        }
        xrandr_text = str(baseline_state["text"])

        fvwm_process, collector = start_fvwm(
            runtime, fvwm_path, display, time.monotonic() + 10.0
        )
        last_status = wait_for_status_monitors(
            collector,
            fvwm_process,
            baseline_monitors,
            output_ids,
            1,
            time.monotonic() + 10.0,
        )
        assert_print_info_topology(
            runtime.root / "fvwm.log",
            last_status,
            time.monotonic() + 5.0,
            fvwm_process,
        )
        baseline_status = json.loads(json.dumps(last_status))

        mutate(["--fbmm", "900x300"], "physical-mm-900x300")
        wait_for_authoritative_state(
            runtime,
            display,
            "root physical size command 900x300",
            lambda state: state.get("root") == {
                "width": 3200,
                "height": 1080,
                "mm_width": 900,
                "mm_height": 300,
            },
            time.monotonic() + 2.0,
            "xrandr-root-mm-900",
        )
        # Xorg-dummy's automatic Monitors retain their output millimetres when
        # only the Screen size changes.  Update the driver's explicit physical
        # properties in the same generation so both levels expose the change.
        mutate(
            [
                "--output", "DUMMY0", "--set", "WIDTH_MM", "540",
                "--set", "HEIGHT_MM", "300",
                "--output", "DUMMY1", "--set", "WIDTH_MM", "360",
                "--set", "HEIGHT_MM", "284",
            ],
            "physical-mm-900x300-output-properties",
        )
        state_mm_900, _ = settle(
            "root physical size 900x300",
            2,
            lambda state: state.get("root") == {
                "width": 3200,
                "height": 1080,
                "mm_width": 900,
                "mm_height": 300,
            }
            and set(state_monitors(state)) == {"DUMMY0", "DUMMY1"}
            and any(
                (monitor["mm_width"], monitor["mm_height"])
                != baseline_mm[name]
                for name, monitor in state_monitors(state).items()
            ),
        )

        mutate(["--fbmm", "846x286"], "physical-mm-846x286")
        mutate(
            [
                "--output", "DUMMY0", "--set", "WIDTH_MM",
                str(baseline_mm["DUMMY0"][0]), "--set", "HEIGHT_MM",
                str(baseline_mm["DUMMY0"][1]),
                "--output", "DUMMY1", "--set", "WIDTH_MM",
                str(baseline_mm["DUMMY1"][0]), "--set", "HEIGHT_MM",
                str(baseline_mm["DUMMY1"][1]),
            ],
            "physical-mm-846x286-output-properties",
        )
        state_mm_restored, _ = settle(
            "restored root physical size",
            3,
            lambda state: state.get("root") == {
                "width": 3200,
                "height": 1080,
                "mm_width": 846,
                "mm_height": 286,
            }
            and set(state_monitors(state)) == {"DUMMY0", "DUMMY1"}
            and all(
                (monitor["mm_width"], monitor["mm_height"])
                == baseline_mm[name]
                for name, monitor in state_monitors(state).items()
            ),
        )

        span_name = "fvwm-test-span"
        span_846 = [
            "--setmonitor", span_name, "3200/846x1080/286+0+0",
            "DUMMY0,DUMMY1",
        ]
        mutate(span_846, "span-create")
        state_span, _ = settle(
            "multi-output Monitor creation",
            4,
            lambda state: state_monitors(state).get(span_name)
            == expected_monitor(
                span_name, primary=False, width=3200, height=1080, x=0, y=0,
                mm_width=846, mm_height=286,
                output_names=["DUMMY0", "DUMMY1"],
            ),
        )
        expected_span_outputs = sorted(output_ids[name]
                                       for name in ("DUMMY0", "DUMMY1"))
        if topology_monitor(last_status, span_name)["outputs"] != expected_span_outputs:
            raise HarnessError("span Status membership is not canonical")

        span_900 = [
            "--setmonitor", span_name, "3200/900x1080/300+0+0",
            "DUMMY0,DUMMY1",
        ]
        pair_position = collector_position(collector)
        mutate(
            ["--delmonitor", span_name, *span_900],
            "span-mm-delete-recreate",
        )
        collector.read_available()
        if len(collector.records) != pair_position:
            raise HarnessError("span mutation emitted an intermediate Status epoch")
        state_span_900, _ = settle(
            "same-name Monitor millimetre mutation",
            5,
            lambda state: state_monitors(state).get(span_name)
            == expected_monitor(
                span_name, primary=False, width=3200, height=1080, x=0, y=0,
                mm_width=900, mm_height=300,
                output_names=["DUMMY0", "DUMMY1"],
            ),
        )

        # A user Monitor claiming both outputs suppresses Xorg's automatic
        # per-output Monitors.  Release it before proving automatic output
        # mutations, then restore the forward membership before the canonical
        # reorder assertions below.
        mutate(["--delmonitor", span_name], "span-release-automatic-monitors")
        state_automatic, _ = settle(
            "automatic Monitors restored after span release",
            6,
            lambda state: set(state_monitors(state)) == {"DUMMY0", "DUMMY1"},
        )

        mutate(
            ["--output", "DUMMY1", "--mode", "1024x768", "--pos", "1920x0"],
            "dummy1-mode-position",
        )
        state_mode, _ = settle(
            "DUMMY1 automatic Monitor mode and position",
            7,
            lambda state: state_monitors(state).get("DUMMY1", {}).get("width")
            == 1024
            and state_monitors(state).get("DUMMY1", {}).get("height") == 768
            and state_monitors(state).get("DUMMY1", {}).get("x") == 1920
            and state_monitors(state).get("DUMMY1", {}).get("y") == 0,
        )
        mutate(["--output", "DUMMY1", "--primary"], "dummy1-primary")
        state_primary, status_primary = settle(
            "DUMMY1 automatic Monitor primary",
            8,
            lambda state: state_monitors(state).get("DUMMY1", {}).get("primary")
            is True
            and state_monitors(state).get("DUMMY0", {}).get("primary") is False,
        )
        mutate(span_900, "span-restore-forward-membership")
        state_span_forward, status_span_forward = settle(
            "forward multi-output Monitor before reorder",
            9,
            lambda state: state_monitors(state).get(span_name)
            == expected_monitor(
                span_name, primary=False, width=3200, height=1080, x=0, y=0,
                mm_width=900, mm_height=300,
                output_names=["DUMMY0", "DUMMY1"],
            ),
        )
        reorder_epoch = topology_epoch(status_span_forward)
        canonical_before_reorder = json.loads(
            json.dumps(status_topology(status_span_forward))
        )

        reversed_span = [
            "--setmonitor", span_name, "3200/900x1080/300+0+0",
            "DUMMY1,DUMMY0",
        ]
        reorder_position = collector_position(collector)
        mutate(
            ["--delmonitor", span_name, *reversed_span],
            "span-reorder-delete-recreate",
        )
        reversed_state = wait_for_authoritative_state(
            runtime,
            display,
            "reversed textual Monitor membership",
            lambda state: state_monitors(state).get(span_name, {}).get(
                "output_names"
            )
            == ["DUMMY1", "DUMMY0"],
            time.monotonic() + 2.0,
            "xrandr-reversed-membership",
        )
        xrandr_text = str(reversed_state["text"])
        assert_no_status_after(
            collector,
            reorder_position,
            time.monotonic() + quiet_seconds,
            fvwm_process,
            "canonical membership reorder",
        )
        if collector.last_status is None:
            raise HarnessError("Status disappeared during canonical reorder")
        if status_topology(collector.last_status) != canonical_before_reorder:
            raise HarnessError("canonical reorder changed Status topology")

        for duplicate_index in range(2):
            mutate(
                ["--delmonitor", span_name, *reversed_span],
                f"span-duplicate-{duplicate_index}-delete-recreate",
            )
        assert_no_status_after(
            collector,
            reorder_position,
            time.monotonic() + quiet_seconds,
            fvwm_process,
            "duplicate equal Monitor notifications",
        )
        if topology_epoch(collector.last_status or {}) != reorder_epoch:
            raise HarnessError("equal duplicate notifications advanced epoch")

        mutate(["--delmonitor", span_name], "span-remove")
        state_removed, _ = settle(
            "user Monitor removal",
            reorder_epoch + 1,
            lambda state: span_name not in state_monitors(state),
        )

        reset_arguments = [
            "--output", "DUMMY0", "--primary", "--output", "DUMMY1",
            "--mode", "1280x1024", "--pos", "1920x0",
        ]
        mutate(reset_arguments, "burst-reset")
        state_reset, status_reset = settle(
            "rapid-burst starting state",
            reorder_epoch + 2,
            lambda state: state_monitors(state).get("DUMMY0", {}).get("primary")
            is True
            and state_monitors(state).get("DUMMY1", {}).get("width") == 1280
            and state_monitors(state).get("DUMMY1", {}).get("height") == 1024
            and state_monitors(state).get("DUMMY1", {}).get("x") == 1920
            and state_monitors(state).get("DUMMY1", {}).get("y") == 0,
        )
        burst_epoch = topology_epoch(status_reset)
        burst_position = collector_position(collector)
        burst_steps = (
            (["--output", "DUMMY1", "--pos", "1600x0"], 1280, 1024, 1600, 0),
            (["--output", "DUMMY1", "--mode", "1024x768"], 1024, 768, 1600, 0),
            (["--output", "DUMMY1", "--pos", "1800x120"], 1024, 768, 1800, 120),
            (["--output", "DUMMY1", "--primary"], 1024, 768, 1800, 120),
        )
        burst_started: float | None = None
        final_burst_state: dict[str, object] | None = None
        os.kill(fvwm_process.process.pid, signal.SIGSTOP)
        try:
            for burst_index, (arguments, width, height, x, y) in enumerate(
                burst_steps
            ):
                started, _ = mutate(
                    arguments, f"rapid-burst-{burst_index}"
                )
                if burst_started is None:
                    burst_started = started
                final_burst_state = wait_for_authoritative_state(
                    runtime,
                    display,
                    f"rapid-burst intermediate {burst_index}",
                    lambda state, width=width, height=height, x=x, y=y,
                    burst_index=burst_index: (
                        state_monitors(state).get("DUMMY1", {}).get("width")
                        == width
                        and state_monitors(state).get("DUMMY1", {}).get("height")
                        == height
                        and state_monitors(state).get("DUMMY1", {}).get("x") == x
                        and state_monitors(state).get("DUMMY1", {}).get("y") == y
                        and (
                            burst_index != len(burst_steps) - 1
                            or state_monitors(state).get("DUMMY1", {}).get(
                                "primary"
                            )
                            is True
                        )
                    ),
                    time.monotonic() + 1.0,
                    f"xrandr-burst-{burst_index}",
                )
                collector.read_available()
                if len(collector.records) != burst_position:
                    raise HarnessError(
                        "rapid burst emitted an intermediate Status epoch"
                    )
        finally:
            os.kill(fvwm_process.process.pid, signal.SIGCONT)
        if burst_started is None:
            raise HarnessError("rapid burst did not start")
        if final_burst_state is None:
            raise HarnessError("rapid burst produced no final queried state")
        last_status = wait_for_status_monitors(
            collector,
            fvwm_process,
            state_monitors(final_burst_state),
            state_output_ids(final_burst_state),
            burst_epoch + 1,
            time.monotonic() + 2.0,
        )

        pressure_name = "fvwm-pressure"
        pressure_forward = [
            "--delmonitor", pressure_name, "--setmonitor", pressure_name,
            "3200/900x1080/300+0+0", "none",
        ]
        pressure_reverse = [
            "--delmonitor", pressure_name, "--setmonitor", pressure_name,
            "3200/900x1080/300+0+0", "none",
        ]
        mutate(
            ["--setmonitor", pressure_name, "3200/900x1080/300+0+0",
             "none"],
            "pressure-create",
        )
        state_pressure, status_pressure = settle(
            "pressure Monitor creation",
            burst_epoch + 2,
            lambda state: pressure_name in state_monitors(state),
        )
        pressure_epoch = topology_epoch(status_pressure)
        pressure_position = collector_position(collector)
        pressure_started, pressure_completed = mutate(
            ["--output", "DUMMY1", "--pos", "1700x60"],
            "pressure-semantic-position",
            maximum_seconds=0.1,
        )
        pressure_deadline = pressure_started + 0.4
        commit_deadline = pressure_started + maximum_ms / 1000.0 + 0.1
        matching_commit_time: float | None = None
        pressure_index = 0
        previous_completed = pressure_completed
        while time.monotonic() < pressure_deadline:
            arguments = pressure_forward if pressure_index % 2 == 0 else pressure_reverse
            started, completed = mutate(
                arguments,
                f"pressure-noise-{pressure_index}",
                maximum_seconds=0.1,
            )
            if started - previous_completed >= quiet_seconds:
                raise HarnessError("sustained pressure command gap reached quiet period")
            previous_completed = completed
            pressure_index += 1
            if fvwm_process.poll() is not None:
                raise HarnessError("FVWM exited during sustained pressure")
            collector.read_available()
            for record in collector.records[pressure_position:]:
                if topology_epoch(record) != pressure_epoch + 1:
                    raise HarnessError("sustained pressure advanced more than one epoch")
                try:
                    monitor = topology_monitor(record, "DUMMY1")
                except HarnessError:
                    continue
                geometry = monitor.get("geometry")
                if type(geometry) is dict and geometry.get("x") == 1700 \
                        and geometry.get("y") == 60:
                    if matching_commit_time is None:
                        matching_commit_time = time.monotonic()
            if matching_commit_time is None and time.monotonic() > commit_deadline:
                raise HarnessError("hard-cap semantic commit missed bounded deadline")
        if pressure_index < 2:
            raise HarnessError("sustained pressure did not issue enough notifications")
        if matching_commit_time is None or matching_commit_time > commit_deadline:
            raise HarnessError("sustained pressure did not expose the bounded commit")

        final_pressure_state = wait_for_authoritative_state(
            runtime,
            display,
            "final canonical pressure state",
            lambda state: pressure_name in state_monitors(state)
            and state_monitors(state).get("DUMMY1", {}).get("x") == 1700
            and state_monitors(state).get("DUMMY1", {}).get("y") == 60,
            time.monotonic() + 2.0,
            "xrandr-pressure-final",
        )
        xrandr_text = str(final_pressure_state["text"])
        last_status = wait_for_status_monitors(
            collector,
            fvwm_process,
            state_monitors(final_pressure_state),
            state_output_ids(final_pressure_state),
            pressure_epoch + 1,
            time.monotonic() + 2.0,
        )
        assert_no_status_after(
            collector,
            collector_position(collector),
            time.monotonic() + quiet_seconds,
            fvwm_process,
            "settled pressure equality",
        )

        scenario_metadata.update(
            {
                "baseline_epoch": 1,
                "final_epoch": topology_epoch(last_status),
                "operations": operations,
                "output_ids": output_ids,
                "pressure_commands": pressure_index,
                "pressure_duration_ms":
                    (previous_completed - pressure_started) * 1000.0,
                "pressure_commit_latency_ms":
                    (matching_commit_time - pressure_started) * 1000.0,
            }
        )
        (artifact_dir / "result.json").write_text(
            json.dumps(scenario_metadata, indent=2, sort_keys=True) + "\n"
        )
    except BaseException as error:
        active_error = error
        if collector is not None:
            last_status = collector.last_status
        if server is not None and server.poll() is None:
            try:
                failed_state = query_authoritative_state(
                    runtime,
                    display,
                    time.monotonic() + 2.0,
                    "xrandr-failure-state",
                )
                xrandr_text = str(failed_state["text"])
                scenario_metadata["failure_root"] = failed_state["root"]
                scenario_metadata["failure_monitors"] = failed_state["monitors"]
            except BaseException as evidence_error:
                scenario_metadata["evidence_error"] = str(evidence_error)
        scenario_metadata["operations"] = operations
        capture_failure(
            artifact_dir,
            error=error,
            server_log=server.log_path if server is not None else None,
            fvwm_log=runtime.root / "fvwm.log",
            xrandr_text=xrandr_text,
            last_status=last_status,
            metadata=scenario_metadata,
        )
        raise
    finally:
        if collector is not None:
            collector.close()
        try:
            runtime.close()
        except BaseException as cleanup_error:
            capture_failure(
                artifact_dir,
                error=cleanup_error,
                server_log=server.log_path if server is not None else None,
                fvwm_log=runtime.root / "fvwm.log",
                xrandr_text=xrandr_text,
                last_status=last_status,
                metadata=scenario_metadata,
            )
            if active_error is None:
                raise


def run_xorg_dummy_failed_query(options: argparse.Namespace) -> None:
    """Prove one failed runtime query retains state and later recovers."""
    required_paths = {
        "--fvwm": options.fvwm,
        "--wrapped-fvwm": options.wrapped_fvwm,
        "--fault-library": options.fault_library,
        "--xorg-config": options.xorg_config,
    }
    for option_name, path in required_paths.items():
        if path is None or not path.is_file():
            raise ValueError(f"{option_name} must name an existing file")
    if options.artifacts_dir is None:
        raise ValueError("--artifacts-dir is required")
    if options.backend != "xorg-dummy":
        raise ValueError("failed-query recovery requires --backend=xorg-dummy")

    fvwm_path = options.fvwm.resolve()
    wrapped_fvwm = options.wrapped_fvwm.resolve()
    fault_library = options.fault_library.resolve()
    config_path = options.xorg_config.resolve()
    artifact_dir = options.artifacts_dir.resolve()
    artifact_dir.mkdir(parents=True, exist_ok=True)
    for artifact_name in (
        "failure.json",
        "fvwm.log",
        "last-status.json",
        "result.json",
        "scenario.json",
        "server.log",
        "xrandr.txt",
    ):
        (artifact_dir / artifact_name).unlink(missing_ok=True)

    runtime = HarnessRuntime(prefix="fvwm-xorg-dummy-failed-query-")
    server: ManagedProcess | None = None
    fvwm_process: ManagedProcess | None = None
    collector: StatusCollector | None = None
    last_status: dict[str, object] | None = None
    xrandr_text: str | None = None
    active_error: BaseException | None = None
    operations: list[dict[str, object]] = []
    attempts: list[dict[str, object]] = []
    metadata: dict[str, object] = {
        "backend": "xorg-dummy",
        "scenario": "failed-query-recovery",
    }
    try:
        server, display = start_xorg_dummy(
            runtime, config_path, time.monotonic() + 10.0
        )
        wait_for_randr(runtime, display, time.monotonic() + 10.0)

        def mutate(arguments: list[str], operation: str) -> None:
            run_required_xrandr(
                runtime, display, arguments, operation, operations
            )

        mutate(
            [
                "--addmode", "DUMMY1", "1280x1024",
                "--addmode", "DUMMY1", "1024x768",
            ],
            "assign-dummy1-modes",
        )
        mutate(
            [
                "--output", "DUMMY0", "--mode", "1920x1080", "--pos", "0x0",
                "--primary", "--output", "DUMMY1", "--mode", "1280x1024",
                "--pos", "1920x0", "--fb", "3200x1080", "--fbmm", "846x286",
            ],
            "baseline",
        )
        baseline_state = wait_for_authoritative_state(
            runtime,
            display,
            "failed-query baseline",
            lambda state: set(state_monitors(state)) == {"DUMMY0", "DUMMY1"}
            and state.get("root") == {
                "width": 3200,
                "height": 1080,
                "mm_width": 846,
                "mm_height": 286,
            },
            time.monotonic() + 5.0,
            "xrandr-failed-query-baseline",
        )
        xrandr_text = str(baseline_state["text"])
        output_ids = state_output_ids(baseline_state)
        selected_mechanism: str | None = None

        for mechanism, executable in (
            ("preload", fvwm_path),
            ("link-wrap", wrapped_fvwm),
        ):
            failed_name = f"fvwm-failed-{mechanism}"
            recovered_name = f"fvwm-recovered-{mechanism}"
            sentinel = runtime.root / f"{mechanism}-runtime.sentinel"
            sentinel.unlink(missing_ok=True)
            overrides = {FAULT_ENV: str(sentinel)}
            if mechanism == "preload":
                overrides[PRELOAD_ENV] = str(fault_library)
            environment = runtime.environment(overrides)
            fvwm_process, collector = start_fvwm(
                runtime,
                executable,
                display,
                time.monotonic() + 10.0,
                environment=environment,
            )
            try:
                known_good = wait_for_status_monitors(
                    collector,
                    fvwm_process,
                    state_monitors(baseline_state),
                    output_ids,
                    1,
                    time.monotonic() + 10.0,
                )
                last_status = known_good
                status_position = collector_position(collector)
                sentinel.touch(mode=0o600, exist_ok=False)
                mutate(
                    [
                        "--setmonitor", failed_name,
                        "800/212x600/159+40+30", "none",
                    ],
                    f"{mechanism}-failed-generation",
                )
                failed_state = wait_for_authoritative_state(
                    runtime,
                    display,
                    f"{mechanism} failed-generation server state",
                    lambda state: failed_name in state_monitors(state),
                    time.monotonic() + 2.0,
                    f"xrandr-{mechanism}-failed-generation",
                )
                xrandr_text = str(failed_state["text"])

                diagnostic = (
                    "RandR monitor query failed; retaining last topology"
                )
                observation_deadline = time.monotonic() + 1.0
                observed = False
                with selectors.DefaultSelector() as selector:
                    selector.register(
                        collector.file_descriptor, selectors.EVENT_READ
                    )
                    while time.monotonic() < observation_deadline:
                        collector.read_available()
                        log_text = _combined_fvwm_log(
                            fvwm_process, runtime.root / "fvwm.log"
                        )
                        diagnostic_count = log_text.count(diagnostic)
                        if not sentinel.exists() and diagnostic_count:
                            observed = True
                            break
                        if sentinel.exists() \
                                and len(collector.records) > status_position:
                            break
                        selector.select(
                            min(
                                0.01,
                                observation_deadline - time.monotonic(),
                            )
                        )

                log_text = _combined_fvwm_log(
                    fvwm_process, runtime.root / "fvwm.log"
                )
                diagnostic_count = log_text.count(diagnostic)
                attempt = {
                    "diagnostics": diagnostic_count,
                    "mechanism": mechanism,
                    "observed": observed,
                    "sentinel_consumed": not sentinel.exists(),
                }
                attempts.append(attempt)
                (artifact_dir / f"{mechanism}-probe.log").write_text(log_text)

                if not observed:
                    if not sentinel.exists() or diagnostic_count:
                        raise HarnessError(
                            f"{mechanism} fault observation was inconsistent"
                        )
                    # The unaffected attempt changed server state.  Restore
                    # the baseline before a fresh FVWM starts at epoch 1.
                    mutate(
                        ["--delmonitor", failed_name],
                        f"{mechanism}-restore-baseline",
                    )
                    baseline_state = wait_for_authoritative_state(
                        runtime,
                        display,
                        f"baseline restored after {mechanism}",
                        lambda state: set(state_monitors(state))
                        == {"DUMMY0", "DUMMY1"},
                        time.monotonic() + 2.0,
                        f"xrandr-{mechanism}-restored-baseline",
                    )
                    continue
                if diagnostic_count != 1:
                    raise HarnessError(
                        f"{mechanism} emitted {diagnostic_count} query diagnostics"
                    )
                collector.read_available()
                if len(collector.records) != status_position:
                    raise HarnessError("failed query emitted downstream Status")
                if collector.last_status is None:
                    raise HarnessError("failed query lost the last-good Status")
                assert_topology_retained(known_good, collector.last_status)
                assert_no_status_after(
                    collector,
                    status_position,
                    time.monotonic() + 0.15,
                    fvwm_process,
                    "failed-query retention",
                )

                mutate(
                    [
                        "--delmonitor", failed_name,
                        "--setmonitor", recovered_name,
                        "960/254x700/185+80+50", "none",
                    ],
                    f"{mechanism}-recovery-generation",
                )
                recovered_state = wait_for_authoritative_state(
                    runtime,
                    display,
                    f"{mechanism} recovered server state",
                    lambda state: failed_name not in state_monitors(state)
                    and recovered_name in state_monitors(state),
                    time.monotonic() + 2.0,
                    f"xrandr-{mechanism}-recovered",
                )
                xrandr_text = str(recovered_state["text"])
                last_status = wait_for_status_monitors(
                    collector,
                    fvwm_process,
                    state_monitors(recovered_state),
                    state_output_ids(recovered_state),
                    2,
                    time.monotonic() + 2.0,
                )
                if sentinel.exists():
                    raise HarnessError("selected fault sentinel was not consumed")

                stable_position = collector_position(collector)
                mutate(
                    [
                        "--delmonitor", recovered_name,
                        "--setmonitor", recovered_name,
                        "960/254x700/185+80+50", "none",
                    ],
                    f"{mechanism}-post-recovery-equal-generation",
                )
                assert_no_status_after(
                    collector,
                    stable_position,
                    time.monotonic() + 0.15,
                    fvwm_process,
                    "post-recovery equal generation",
                )
                if topology_epoch(collector.last_status or {}) != 2:
                    raise HarnessError("post-recovery query advanced the epoch")
                selected_mechanism = mechanism
                break
            finally:
                if collector is not None:
                    collector.close()
                    collector = None
                fvwm_process.close(time.monotonic() + 2.0)
                fvwm_process = None

        if selected_mechanism is None:
            raise HarnessError("neither preload nor link-wrap failed a runtime query")
        metadata.update(
            {
                "attempts": attempts,
                "mechanism": selected_mechanism,
                "operations": operations,
                "output_ids": output_ids,
                "retained_epoch": 1,
                "recovered_epoch": topology_epoch(last_status or {}),
            }
        )
        (artifact_dir / "result.json").write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + "\n"
        )
    except BaseException as error:
        active_error = error
        if collector is not None:
            last_status = collector.last_status
        metadata["attempts"] = attempts
        metadata["operations"] = operations
        capture_failure(
            artifact_dir,
            error=error,
            server_log=server.log_path if server is not None else None,
            fvwm_log=runtime.root / "fvwm.log",
            xrandr_text=xrandr_text,
            last_status=last_status,
            metadata=metadata,
        )
        raise
    finally:
        if collector is not None:
            collector.close()
        try:
            runtime.close()
        except BaseException as cleanup_error:
            capture_failure(
                artifact_dir,
                error=cleanup_error,
                server_log=server.log_path if server is not None else None,
                fvwm_log=runtime.root / "fvwm.log",
                xrandr_text=xrandr_text,
                last_status=last_status,
                metadata=metadata,
            )
            if active_error is None:
                raise


def run_xvfb_topology(options: argparse.Namespace) -> None:
    """Prove the mandatory Xvfb startup and epoch-1 inspection baseline."""
    if options.fvwm is None or not options.fvwm.is_file():
        raise ValueError("--fvwm must name an existing file")
    if options.artifacts_dir is None:
        raise ValueError("--artifacts-dir is required")
    if options.backend != "xvfb":
        raise ValueError("topology scenario currently requires --backend=xvfb")

    fvwm_path = options.fvwm.resolve()
    artifact_dir = options.artifacts_dir.resolve()
    artifact_dir.mkdir(parents=True, exist_ok=True)
    for artifact_name in (
        "failure.json",
        "fvwm.log",
        "last-status.json",
        "result.json",
        "server.log",
        "xrandr.txt",
    ):
        (artifact_dir / artifact_name).unlink(missing_ok=True)
    runtime = HarnessRuntime(prefix="fvwm-xvfb-topology-")
    server: ManagedProcess | None = None
    fvwm_process: ManagedProcess | None = None
    collector: StatusCollector | None = None
    xrandr_text: str | None = None
    last_status: dict[str, object] | None = None
    active_error: BaseException | None = None
    try:
        server, display = start_display_server(
            runtime,
            "Xvfb",
            ["Xvfb", "-screen", "0", "1280x720x24"],
            "xserver.log",
            time.monotonic() + 10.0,
        )
        expected_server_argv = [
            "Xvfb",
            "-displayfd",
            server.argv[server.argv.index("-displayfd") + 1],
            "-nolisten",
            "tcp",
            "-screen",
            "0",
            "1280x720x24",
        ]
        if server.argv != expected_server_argv:
            raise HarnessError(f"unexpected Xvfb argv: {server.argv!r}")
        wait_for_randr(runtime, display, time.monotonic() + 10.0)

        return_code, version_text = query_xrandr(
            runtime,
            display,
            ["--version"],
            time.monotonic() + 5.0,
            "xrandr-version.log",
        )
        if return_code != 0:
            raise HarnessError("xrandr --version failed")
        randr_version = parse_randr_version(version_text)
        if randr_version < (1, 5):
            raise HarnessError(
                f"RandR 1.5 or newer is required, server has {randr_version}"
            )

        return_code, xrandr_text = query_xrandr(
            runtime,
            display,
            ["--listmonitors"],
            time.monotonic() + 5.0,
            "xrandr-listmonitors-baseline.log",
        )
        if return_code != 0:
            raise HarnessError("xrandr --listmonitors failed")
        monitors = parse_xrandr_monitors(xrandr_text)
        if set(monitors) != {"screen"}:
            raise HarnessError("Xvfb baseline must expose only Monitor 'screen'")
        baseline_monitor = monitors["screen"]
        expected_geometry = {"x": 0, "y": 0, "width": 1280, "height": 720}
        if any(
            baseline_monitor[key] != value
            for key, value in expected_geometry.items()
        ):
            raise HarnessError("Xvfb screen Monitor has unexpected geometry")
        if baseline_monitor["output_names"] != ["screen"]:
            raise HarnessError("Xvfb screen Monitor has unexpected membership")

        return_code, verbose_text = query_xrandr(
            runtime,
            display,
            ["--verbose"],
            time.monotonic() + 5.0,
            "xrandr-verbose-baseline.log",
        )
        if return_code != 0:
            raise HarnessError("xrandr --verbose failed")
        output_ids = parse_xrandr_output_ids(verbose_text)
        if "screen" not in output_ids:
            raise HarnessError("Xvfb output 'screen' has no queried XID")
        baseline_outputs = [output_ids["screen"]]

        fvwm_process, collector = start_fvwm(
            runtime, fvwm_path, display, time.monotonic() + 10.0
        )

        def baseline_status(record: dict[str, object]) -> bool:
            try:
                assert_status_monitor_matches(
                    record,
                    baseline_monitor,
                    baseline_outputs,
                    expected_epoch=1,
                )
            except HarnessError:
                return False
            return len(status_topology(record)["monitors"]) == 1

        last_status = collector.wait_for(
            "strict Status v4 epoch 1",
            baseline_status,
            time.monotonic() + 10.0,
            process=fvwm_process,
        )
        baseline_status_record = last_status

        fvwm_log = runtime.root / "fvwm.log"

        def print_info_visible() -> bool:
            if not fvwm_log.is_file():
                return False
            text = fvwm_log.read_text(errors="replace")
            return (
                "Topology epoch=1 monitors=1" in text
                and 'Topology monitor name="screen"' in text
            )

        wait_for_predicate(
            "PrintInfo Topology epoch and Monitor",
            print_info_visible,
            time.monotonic() + 5.0,
            process=fvwm_process,
        )
        assert_status_stable(
            collector,
            baseline_status_record,
            time.monotonic() + 0.35,
            fvwm_process,
        )
        last_status = collector.last_status

        operations: list[dict[str, object]] = []

        def run_mutation(arguments: list[str], operation: str) -> None:
            return_code, output = query_xrandr(
                runtime,
                display,
                arguments,
                time.monotonic() + 5.0,
                f"xrandr-{operation}.log",
            )
            operations.append(
                {
                    "argv": ["xrandr", "--display", display, *arguments],
                    "operation": operation,
                    "returncode": return_code,
                    "output": output,
                }
            )

        probe_name = "fvwm-xvfb-probe"
        create_arguments = [
            "--setmonitor",
            probe_name,
            "640/170x720/191+0+0",
            "screen",
        ]
        delete_arguments = ["--delmonitor", probe_name]
        recreate_arguments = [
            "--setmonitor",
            probe_name,
            "800/212x600/159+0+0",
            "none",
        ]
        run_mutation(create_arguments, "create")
        created = poll_xrandr_monitors(
            runtime,
            display,
            "Xvfb Monitor creation capability",
            lambda state: probe_name in state,
            time.monotonic() + 2.0,
            "xrandr-create-state",
        )

        if created is None:
            mutation_capability = "unsupported"
            for operation, arguments in (
                ("mutation-delete", delete_arguments),
                ("mutation-recreate", recreate_arguments),
                ("remove", delete_arguments),
            ):
                run_mutation(arguments, operation)
                unchanged = poll_xrandr_monitors(
                    runtime,
                    display,
                    f"unchanged baseline after {operation}",
                    lambda state: state == monitors,
                    time.monotonic() + 2.0,
                    f"xrandr-{operation}-state",
                )
                if unchanged is None:
                    raise HarnessError(
                        f"Xvfb Monitor state changed after unsupported {operation}"
                    )
                _, xrandr_text = unchanged
                assert_status_stable(
                    collector,
                    baseline_status_record,
                    time.monotonic() + 0.35,
                    fvwm_process,
                )
            print("UNSUPPORTED: xvfb-monitor-mutation")
        else:
            mutation_capability = "supported"
            created_monitors, xrandr_text = created
            expected_created = {
                "name": probe_name,
                "primary": False,
                "x": 0,
                "y": 0,
                "width": 640,
                "height": 720,
                "mm_width": 170,
                "mm_height": 191,
                "output_names": ["screen"],
            }
            if created_monitors.get(probe_name) != expected_created:
                raise HarnessError("created Xvfb Monitor facts are not exact")
            last_status = wait_for_status_monitors(
                collector,
                fvwm_process,
                created_monitors,
                output_ids,
                2,
                time.monotonic() + 2.0,
            )

            if fvwm_process.poll() is not None:
                raise HarnessError("FVWM exited before supported Monitor mutation")
            os.kill(fvwm_process.process.pid, signal.SIGSTOP)
            try:
                run_mutation(delete_arguments, "mutation-delete")
                run_mutation(recreate_arguments, "mutation-recreate")
            finally:
                os.kill(fvwm_process.process.pid, signal.SIGCONT)
            expected_mutated = {
                "name": probe_name,
                "primary": False,
                "x": 0,
                "y": 0,
                "width": 800,
                "height": 600,
                "mm_width": 212,
                "mm_height": 159,
                "output_names": [],
            }
            mutated = poll_xrandr_monitors(
                runtime,
                display,
                "exact mutated Xvfb Monitor facts",
                lambda state: state.get(probe_name) == expected_mutated,
                time.monotonic() + 2.0,
                "xrandr-mutation-state",
            )
            if mutated is None:
                raise HarnessError("supported Xvfb Monitor mutation did not settle")
            mutated_monitors, xrandr_text = mutated
            last_status = wait_for_status_monitors(
                collector,
                fvwm_process,
                mutated_monitors,
                output_ids,
                3,
                time.monotonic() + 2.0,
            )

            run_mutation(delete_arguments, "remove")
            removed = poll_xrandr_monitors(
                runtime,
                display,
                "removed Xvfb Monitor",
                lambda state: probe_name not in state,
                time.monotonic() + 2.0,
                "xrandr-remove-state",
            )
            if removed is None:
                raise HarnessError("supported Xvfb Monitor removal did not settle")
            removed_monitors, xrandr_text = removed
            last_status = wait_for_status_monitors(
                collector,
                fvwm_process,
                removed_monitors,
                output_ids,
                4,
                time.monotonic() + 2.0,
            )

        result = {
            "backend": "xvfb",
            "baseline_epoch": 1,
            "monitor": baseline_monitor,
            "mutation_capability": mutation_capability,
            "operations": operations,
            "output_ids": output_ids,
            "randr_version": list(randr_version),
        }
        (artifact_dir / "result.json").write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n"
        )
    except BaseException as error:
        active_error = error
        if collector is not None:
            last_status = collector.last_status
        capture_failure(
            artifact_dir,
            error=error,
            server_log=server.log_path if server is not None else None,
            fvwm_log=runtime.root / "fvwm.log",
            xrandr_text=xrandr_text,
            last_status=last_status,
            metadata={"backend": "xvfb", "scenario": "topology"},
        )
        raise
    finally:
        if collector is not None:
            collector.close()
        try:
            runtime.close()
        except BaseException as cleanup_error:
            capture_failure(
                artifact_dir,
                error=cleanup_error,
                server_log=server.log_path if server is not None else None,
                fvwm_log=runtime.root / "fvwm.log",
                xrandr_text=xrandr_text,
                last_status=last_status,
                metadata={"backend": "xvfb", "scenario": "topology"},
            )
            if active_error is None:
                raise


def capture_failure(
    artifact_dir: Path,
    *,
    error: BaseException,
    server_log: Path | None,
    fvwm_log: Path | None,
    xrandr_text: str | None,
    last_status: dict[str, object] | None,
    metadata: dict[str, object] | None = None,
) -> None:
    """Retain bounded diagnostic inputs before callers tear processes down."""
    artifact_dir.mkdir(parents=True, exist_ok=True)
    failure = {"error": type(error).__name__, "message": str(error)}
    (artifact_dir / "failure.json").write_text(
        json.dumps(failure, indent=2, sort_keys=True) + "\n"
    )
    if metadata is not None:
        (artifact_dir / "scenario.json").write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + "\n"
        )
    for source, destination in (
        (server_log, artifact_dir / "server.log"),
        (fvwm_log, artifact_dir / "fvwm.log"),
    ):
        if source is not None and source.is_file():
            destination.write_bytes(source.read_bytes())
    if xrandr_text is not None:
        (artifact_dir / "xrandr.txt").write_text(xrandr_text)
    if last_status is not None:
        (artifact_dir / "last-status.json").write_text(
            json.dumps(last_status, indent=2, sort_keys=True) + "\n"
        )


def test_xorg_dummy_contract() -> None:
    """Specify the authoritative backend's argv and parsing contracts."""
    config_path = Path("/private/xorg-dummy.conf")
    assert xorg_dummy_server_argv(config_path, 19) == [
        "Xorg",
        "-config",
        str(config_path),
        "-displayfd",
        "19",
        "-nolisten",
        "tcp",
        "-noreset",
    ]

    policy_header = """\
#define FVWM_TOPOLOGY_QUIET_MS 50
#define FVWM_TOPOLOGY_MAX_LATENCY_MS 250
"""
    assert parse_topology_policy_constants(policy_header) == (50, 250)

    query_text = """\
Screen 0: minimum 64 x 64, current 3200 x 1080, maximum 32767 x 32767
DUMMY0 connected primary 1920x1080+0+0 (normal) 508mm x 286mm
DUMMY1 connected 1280x1024+1920+0 (normal) 338mm x 271mm
"""
    assert parse_xrandr_root_dimensions(query_text) == {
        "width": 3200,
        "height": 1080,
        "mm_width": 846,
        "mm_height": 286,
    }


def test_failed_query_recovery_contract() -> None:
    """Specify independent recovery selection and last-good retention."""
    options = build_parser().parse_args(
        ["--backend=xorg-dummy", "--scenario=failed-query-recovery"]
    )
    assert options.scenario == "failed-query-recovery"

    known_good = {
        "version": 4,
        "topology": {"epoch": 7, "monitors": [{"name": "DUMMY0"}]},
    }
    retained = json.loads(json.dumps(known_good))
    assert_topology_retained(known_good, retained)
    retained["topology"]["epoch"] = 8
    try:
        assert_topology_retained(known_good, retained)
    except HarnessError:
        pass
    else:
        raise AssertionError("changed failed-query topology must be rejected")


def test_fvwm_arguments_extra_commands() -> None:
    """Specify argv-only startup command ordering and byte preservation."""
    fvwm_path = Path("/private/fvwm3")
    log_path = Path("/private/fvwm.log")
    baseline = [
        str(fvwm_path),
        "-d",
        ":77",
        "-o",
        str(log_path),
        "-v",
        "-c",
        "Schedule 0 Status On",
        "-c",
        "PrintInfo Topology",
    ]
    assert fvwm_arguments(fvwm_path, ":77", log_path) == baseline
    assert fvwm_arguments(
        fvwm_path,
        ":77",
        log_path,
        extra_commands=("MonitorScale DUMMY0 1.25",),
    ) == [*baseline, "-c", "MonitorScale DUMMY0 1.25"]
    quoted = 'MonitorScale "name with spaces and \\"quotes\\"" 2.5'
    assert fvwm_arguments(
        fvwm_path,
        ":77",
        log_path,
        extra_commands=("MonitorScaleFallback 1.75", quoted),
    ) == [
        *baseline,
        "-c",
        "MonitorScaleFallback 1.75",
        "-c",
        quoted,
    ]


def test_post_finalize_scale_inspection_argv() -> None:
    """Specify the scheduled inspection as one final argv command pair."""
    arguments = fvwm_arguments(
        Path("/private/fvwm3"),
        ":77",
        Path("/private/fvwm.log"),
        extra_commands=(
            "MonitorScaleFallback 1.5",
            'MonitorScale "DUMMY 0" 2',
        ),
    )
    expected_prefix = list(arguments)
    add_post_finalize_scale_inspection(arguments)
    assert arguments == [
        *expected_prefix,
        "-c",
        "Schedule 0 PrintInfo Scale",
    ]


def test_print_info_scale_parser() -> None:
    """Specify strict active/unmatched parsing and escaped-name identity."""
    escaped_name = 'quote"line\n\t\x01-é'
    active = (
        f"Scale monitor name={json.dumps(escaped_name)} factor=1.25 "
        "source=override pixels=1920x1080 physical_mm=254x143 "
        "match_status=unique auto_status=valid boundary=none "
        "reason=exact-override"
    )
    assert parse_print_info_scale_line(active) == {
        "kind": "monitor",
        "name": escaped_name,
        "factor": "1.25",
        "source": "override",
        "pixels": (1920, 1080),
        "physical_mm": (254, 143),
        "match_status": "unique",
        "auto_status": "valid",
        "boundary": "none",
        "reason": "exact-override",
    }
    unmatched = (
        f"Scale override name={json.dumps(escaped_name)} factor=2.75 "
        "match_status=none state=retained-unmatched"
    )
    assert parse_print_info_scale_line(unmatched) == {
        "kind": "override",
        "name": escaped_name,
        "factor": "2.75",
        "match_status": "none",
        "state": "retained-unmatched",
    }
    malformed = (
        active.replace(" reason=exact-override", "")
        + " source=automatic reason=exact-override"
    )
    for line in (
        active.replace(" reason=exact-override", ""),
        active.replace("factor=1.25", "factor=1.30"),
        active.replace("source=override", "source=override source=automatic"),
        malformed,
        unmatched.replace(" state=retained-unmatched", ""),
    ):
        try:
            parse_print_info_scale_line(line)
        except HarnessError:
            pass
        else:
            raise AssertionError(f"malformed scale line must be rejected: {line!r}")


def self_test() -> None:
    """Specify lifecycle behavior without requiring an X server."""
    test_xorg_dummy_contract()
    test_failed_query_recovery_contract()
    test_fvwm_arguments_extra_commands()
    test_post_finalize_scale_inspection_argv()
    test_print_info_scale_parser()

    version_text = """\
xrandr program version       1.5.1
Server reports RandR version 1.6
"""
    assert parse_randr_version(version_text) == (1, 6)

    monitor_text = """\
Monitors: 1
 0: +screen 1280/339x720/190+0+0  screen
"""
    monitors = parse_xrandr_monitors(monitor_text)
    assert monitors == {
        "screen": {
            "name": "screen",
            "primary": False,
            "x": 0,
            "y": 0,
            "width": 1280,
            "height": 720,
            "mm_width": 339,
            "mm_height": 190,
            "output_names": ["screen"],
        }
    }

    verbose_text = """\
screen connected 1280x720+0+0 (0x3a) normal (normal) 0mm x 0mm
\tIdentifier: 0x3b
\tTimestamp:  1
"""
    assert parse_xrandr_output_ids(verbose_text) == {"screen": 0x3B}

    status_fixture = {
        "version": 4,
        "topology": {
            "epoch": 1,
            "monitors": [
                {
                    "name": "screen",
                    "name_atom": 240,
                    "primary": False,
                    "geometry": {
                        "x": 0,
                        "y": 0,
                        "width": 1280,
                        "height": 720,
                    },
                    "physical_mm": {"width": 339, "height": 190},
                    "outputs": [0x3B],
                }
            ],
        },
    }
    assert_status_monitor_matches(
        status_fixture,
        monitors["screen"],
        [0x3B],
        expected_epoch=1,
    )

    topology_options = build_parser().parse_args(
        ["--backend=xvfb", "--scenario=topology"]
    )
    assert topology_options.scenario == "topology"

    with tempfile.TemporaryDirectory(prefix="fvwm-live-x-self-test-") as name:
        private_tmpdir = Path(name)
        old_fault = os.environ.get(FAULT_ENV)
        os.environ[FAULT_ENV] = "must-not-leak"
        try:
            environment = sanitized_environment(private_tmpdir)
        finally:
            if old_fault is None:
                os.environ.pop(FAULT_ENV, None)
            else:
                os.environ[FAULT_ENV] = old_fault
        assert environment["TMPDIR"] == str(private_tmpdir)
        assert FAULT_ENV not in environment
        assert PRELOAD_ENV not in environment

        # The implementation must launch an argv vector in a private session,
        # retain its log, and reap it within a monotonic hard deadline.
        process = ManagedProcess.start(
            "self-test",
            [os.sys.executable, "-c", "print('child-ready', end='')"],
            private_tmpdir / "child.log",
            environment,
        )
        assert process.argv == [
            os.sys.executable,
            "-c",
            "print('child-ready', end='')",
        ]
        assert process.wait_until_exit(time.monotonic() + 2.0) == 0
        process.close(time.monotonic() + 2.0)
        assert (private_tmpdir / "child.log").read_text() == "child-ready"

        attempts = 0

        def becomes_ready() -> str | None:
            nonlocal attempts
            attempts += 1
            return "ready" if attempts == 3 else None

        assert wait_for_predicate(
            "self-test predicate", becomes_ready, time.monotonic() + 1.0
        ) == "ready"

        with HarnessRuntime(prefix="fvwm-live-x-display-self-test-") as runtime:
            display_script = (
                "#!/usr/bin/env python3\n"
                "import os, signal, sys\n"
                "fd = int(sys.argv[sys.argv.index('-displayfd') + 1])\n"
                "os.write(fd, b'77\\n')\n"
                "os.close(fd)\n"
                "signal.pause()\n"
            )
            fake_server = runtime.root / "fake-xserver"
            fake_server.write_text(display_script)
            fake_server.chmod(0o700)
            server, display = start_display_server(
                runtime,
                "fake-Xserver",
                [str(fake_server)],
                "server.log",
                time.monotonic() + 2.0,
            )
            assert display == ":77"
            assert "-nolisten" in server.argv
            assert server.argv[server.argv.index("-nolisten") + 1] == "tcp"
            assert server.argv[server.argv.index("-displayfd") + 1].isdigit()

        fifo_path = private_tmpdir / "fvwm3.pipe"
        os.mkfifo(fifo_path)
        with StatusCollector.open(fifo_path, time.monotonic() + 1.0) as collector:
            writer = os.open(fifo_path, os.O_WRONLY | os.O_NONBLOCK)
            try:
                os.write(
                    writer,
                    b'{"version":4,"topology":{"epoch":1,"monitors":[]}}\n',
                )
            finally:
                os.close(writer)
            status = collector.wait_for(
                "Status epoch 1",
                lambda record: topology_epoch(record) == 1,
                time.monotonic() + 1.0,
            )
            assert topology_epoch(status) == 1

        artifact_dir = private_tmpdir / "artifacts"
        capture_failure(
            artifact_dir,
            error=RuntimeError("expected evidence"),
            server_log=private_tmpdir / "child.log",
            fvwm_log=None,
            xrandr_text="Monitors: 1\n",
            last_status={"version": 4, "topology": {"epoch": 1}},
        )
        assert (artifact_dir / "failure.json").is_file()
        assert json.loads((artifact_dir / "last-status.json").read_text())["version"] == 4

        stubborn_script = (
            "import signal;"
            "signal.signal(signal.SIGTERM,signal.SIG_IGN);"
            "print('ready',flush=True);signal.pause()"
        )
        stubborn = ManagedProcess.start(
            "stubborn-child",
            [os.sys.executable, "-c", stubborn_script],
            private_tmpdir / "stubborn.log",
            environment,
        )
        wait_for_predicate(
            "stubborn child readiness",
            lambda: "ready" in (private_tmpdir / "stubborn.log").read_text(),
            time.monotonic() + 1.0,
            process=stubborn,
        )
        stubborn.close(time.monotonic() + 1.0)
        assert stubborn.poll() == -signal.SIGKILL

        metadata = fault_probe_metadata("preload")
        assert metadata == {"mechanism": "preload"}
        try:
            fault_probe_metadata("invalid")
        except ValueError:
            pass
        else:
            raise AssertionError("invalid fault mechanisms must be rejected")


def main() -> int:
    """Run self-tests or a selected live-X scenario."""
    options = build_parser().parse_args()
    if options.check_workflow is not None:
        check_live_x_workflow(options.check_workflow)
        return 0
    if options.self_test:
        self_test()
        return 0
    if options.backend is None or options.scenario is None:
        build_parser().error("--backend and --scenario are required")
    if options.scenario == "fault-injection-probe":
        run_fault_injection_probe(options)
        return 0
    if options.scenario == "failed-query-recovery":
        run_xorg_dummy_failed_query(options)
        return 0
    if options.scenario == "topology":
        if options.backend == "xvfb":
            run_xvfb_topology(options)
        elif options.backend == "xorg-dummy":
            run_xorg_dummy_topology(options)
        else:
            raise AssertionError(f"unhandled topology backend: {options.backend}")
        return 0
    raise AssertionError(f"unhandled live-X scenario: {options.scenario}")


if __name__ == "__main__":
    raise SystemExit(main())
