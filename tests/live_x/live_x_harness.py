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
import json
import os
from pathlib import Path
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
        "--scenario", choices=("fault-injection-probe",), default=None
    )
    parser.add_argument("--fvwm", type=Path)
    parser.add_argument("--wrapped-fvwm", type=Path)
    parser.add_argument("--fault-library", type=Path)
    parser.add_argument("--artifacts-dir", type=Path)
    parser.add_argument("--self-test", action="store_true")
    return parser


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
        argv = list(argv_prefix)
        argv.extend(["-displayfd", str(write_fd), "-nolisten", "tcp"])
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
    fvwm_path: Path, display: str, fvwm_log: Path
) -> list[str]:
    """Build the fixed FVWM startup argv used by live-X probes."""
    return [
        str(fvwm_path),
        "-d",
        display,
        "-o",
        str(fvwm_log),
        "-v",
        "-c",
        "Status On",
    ]


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


def capture_failure(
    artifact_dir: Path,
    *,
    error: BaseException,
    server_log: Path | None,
    fvwm_log: Path | None,
    xrandr_text: str | None,
    last_status: dict[str, object] | None,
) -> None:
    """Retain bounded diagnostic inputs before callers tear processes down."""
    artifact_dir.mkdir(parents=True, exist_ok=True)
    failure = {"error": type(error).__name__, "message": str(error)}
    (artifact_dir / "failure.json").write_text(
        json.dumps(failure, indent=2, sort_keys=True) + "\n"
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


def self_test() -> None:
    """Specify lifecycle behavior without requiring an X server."""
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
                "import os,signal,sys;"
                "fd=int(sys.argv[sys.argv.index('-displayfd')+1]);"
                "os.write(fd,b'77\\n');os.close(fd);signal.pause()"
            )
            server, display = start_display_server(
                runtime,
                "fake-Xserver",
                [os.sys.executable, "-c", display_script],
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
    if options.self_test:
        self_test()
        return 0
    if options.backend is None or options.scenario is None:
        build_parser().error("--backend and --scenario are required")
    if options.scenario == "fault-injection-probe":
        run_fault_injection_probe(options)
        return 0
    raise AssertionError(f"unhandled live-X scenario: {options.scenario}")


if __name__ == "__main__":
    raise SystemExit(main())
