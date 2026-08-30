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

"""Run native verification directly on Linux or in Podman on macOS."""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Sequence


IMAGE = "fvwmorg/fvwm3-build:latest"
LIVE_X_PACKAGES = (
    "xvfb",
    "x11-xserver-utils",
    "xserver-xorg-core",
    "xserver-xorg-video-dummy",
)
LIVE_X_TEST_NAMES = (
    "scale-policy-xorg-dummy",
    "topology-fault-injection-probe",
    "topology-xorg-dummy",
    "topology-xorg-dummy-failed-query",
    "topology-xvfb",
)
LIVE_X_SUITE = "fvwm3:live-x"
INSTALL_LIVE_X = (
    "apt-get update && apt-get install -y "
    + " ".join(LIVE_X_PACKAGES)
    + " && "
)
LIVE_X_PREFLIGHT = (
    "if test -d builddir/meson-private; then "
    "meson setup builddir --reconfigure -Dlive-x-tests=true; "
    "else meson setup builddir -Dlive-x-tests=true; fi && "
    "meson compile -C builddir && "
    "python3 tests/run_in_linux.py --check-live-x-tests "
    "builddir/meson-info/intro-tests.json && "
)
SYNC_SOURCE = (
    "find /workspace -mindepth 1 -maxdepth 1 ! -name builddir "
    "-exec rm -rf -- {} + && "
    "tar -C /source --exclude=./builddir --exclude=./.git -cf - . "
    "| tar -C /workspace -xf - && "
)


class UnsupportedHostError(RuntimeError):
    """Raised when no supported native-Linux execution route exists."""


def validate_live_x_tests(tests: Sequence[dict[str, object]]) -> None:
    """Require the exact audited membership of Meson's qualified suite."""
    names = [
        test.get("name")
        for test in tests
        if LIVE_X_SUITE in test.get("suite", [])
    ]
    missing = sorted(set(LIVE_X_TEST_NAMES) - set(names))
    extra = sorted(set(names) - set(LIVE_X_TEST_NAMES))
    if missing or extra or len(names) != len(LIVE_X_TEST_NAMES):
        raise ValueError(
            f"invalid {LIVE_X_SUITE} suite membership: "
            f"missing={missing}, extra={extra}, members={sorted(names)}"
        )


def build_command(
    host: str,
    repository: Path,
    caller_argv: Sequence[str],
    live_x: bool = False,
    podman: str = "podman",
) -> list[str]:
    """Construct the command without executing it."""
    if not caller_argv:
        raise ValueError("a command is required after --")
    if host == "Linux":
        if live_x:
            return ["sh", "-ec", LIVE_X_PREFLIGHT + 'exec "$@"', "sh", *caller_argv]
        return list(caller_argv)
    if host != "Darwin":
        raise UnsupportedHostError(f"unsupported host: {host}")

    repository = repository.resolve()
    volume_suffix = hashlib.sha256(str(repository).encode()).hexdigest()[:12]
    command = [
        podman,
        "run",
        "--rm",
        "--platform",
        "linux/amd64",
        "--volume",
        f"{repository}:/source:ro",
        "--volume",
        f"fvwm3-linux-{volume_suffix}:/workspace",
        "--workdir",
        "/workspace",
        IMAGE,
        "sh",
        "-ec",
    ]
    container_script = SYNC_SOURCE
    if live_x:
        container_script += INSTALL_LIVE_X + LIVE_X_PREFLIGHT + 'exec "$@"'
    else:
        container_script += 'exec "$@"'
    command.extend([container_script, "sh"])
    command.extend(caller_argv)
    return command


def self_test() -> None:
    """Exercise every routing branch without starting a subprocess."""
    repository = Path("/work/fvwm3")
    caller_argv = ["sh", "-ec", "exit 23"]

    introspection = [
        {"name": name, "suite": [LIVE_X_SUITE]} for name in LIVE_X_TEST_NAMES
    ]
    validate_live_x_tests(introspection)
    invalid_introspection = (
        introspection[:-1]
        + [{"name": LIVE_X_TEST_NAMES[-1], "suite": ["other:live-x"]}]
    )
    for invalid in (
        invalid_introspection,
        introspection[:-1],
        introspection + [{"name": "unexpected-live-x", "suite": [LIVE_X_SUITE]}],
    ):
        try:
            validate_live_x_tests(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid live-X suite membership must be rejected")

    assert build_command("Linux", repository, caller_argv) == caller_argv
    linux_live_x = build_command("Linux", repository, caller_argv, live_x=True)
    assert linux_live_x[:2] == ["sh", "-ec"]
    assert "-Dlive-x-tests=true" in linux_live_x[2]
    assert "--check-live-x-tests" in linux_live_x[2]
    assert linux_live_x[-len(caller_argv) :] == caller_argv

    darwin = build_command(
        "Darwin", repository, caller_argv, podman="/usr/local/bin/podman"
    )
    assert darwin[:3] == ["/usr/local/bin/podman", "run", "--rm"]
    assert darwin[darwin.index("--platform") + 1] == "linux/amd64"
    assert darwin[darwin.index("--workdir") + 1] == "/workspace"
    assert f"{repository}:/source:ro" in darwin
    assert any(argument.startswith("fvwm3-linux-") for argument in darwin)
    container_script = darwin[darwin.index("-ec") + 1]
    assert container_script.index("find /workspace") < container_script.index("tar -C")
    assert "! -name builddir" in container_script
    assert darwin[-len(caller_argv) :] == caller_argv
    assert IMAGE in darwin

    hostile_argv = ["printf", "%s", "not; shell $(input)"]
    live_x = build_command("Darwin", repository, hostile_argv, live_x=True)
    install_fragment = live_x[live_x.index("-ec") + 1]
    install_words = install_fragment.split()
    package_start = install_words.index("-y") + 1
    package_end = install_words.index("&&", package_start)
    assert tuple(install_words[package_start:package_end]) == LIVE_X_PACKAGES
    assert "-Dlive-x-tests=true" in install_fragment
    assert "--check-live-x-tests" in install_fragment
    assert hostile_argv[-1] not in install_fragment
    assert live_x[-len(hostile_argv) :] == hostile_argv

    try:
        build_command("Windows", repository, caller_argv)
    except UnsupportedHostError:
        pass
    else:
        raise AssertionError("unsupported hosts must be rejected")


def parse_args(argv: Sequence[str]) -> tuple[argparse.Namespace, list[str]]:
    """Parse router options and require a literal -- before caller argv."""
    before_separator: Sequence[str]
    command: list[str]
    if "--" in argv:
        separator = argv.index("--")
        before_separator = argv[:separator]
        command = list(argv[separator + 1 :])
    else:
        before_separator = argv
        command = []

    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--live-x", action="store_true")
    parser.add_argument("--check-live-x-tests", type=Path)
    options = parser.parse_args(before_separator)
    if not options.self_test and options.check_live_x_tests is None and not command:
        parser.error("a command is required after --")
    if (options.self_test or options.check_live_x_tests is not None) and command:
        parser.error("self-tests and preflight checks do not accept a command")
    return options, command


def main(argv: Sequence[str] | None = None) -> int:
    """Run self-tests or route the requested command."""
    options, caller_argv = parse_args(sys.argv[1:] if argv is None else argv)
    if options.self_test:
        self_test()
        return 0
    if options.check_live_x_tests is not None:
        try:
            validate_live_x_tests(
                json.loads(options.check_live_x_tests.read_text(encoding="utf-8"))
            )
        except (OSError, json.JSONDecodeError, TypeError, ValueError) as error:
            print(f"error: {error}", file=sys.stderr)
            return 1
        return 0

    host = platform.system()
    podman = shutil.which("podman") if host == "Darwin" else None
    if host == "Darwin" and podman is None:
        print("error: Podman is required for Linux verification on macOS", file=sys.stderr)
        return 127

    try:
        command = build_command(
            host,
            Path(__file__).resolve().parents[1],
            caller_argv,
            live_x=options.live_x,
            podman=podman or "podman",
        )
    except (UnsupportedHostError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    return subprocess.run(command, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
