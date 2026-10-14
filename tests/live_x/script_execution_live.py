#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Exercise real interpreter limits, nested calls and event recovery."""

import argparse
import json
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(options):
    options.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix="fvwm-script-execution-") as runtime:
        connection = None
        try:
            display = options.display
            if display is None:
                _, display = start_display_server(runtime, "Xvfb",
                    ["Xvfb", "-screen", "0", "800x600x24", "-noreset"],
                    "xserver.log", time.monotonic() + 10)
            config = runtime.root / "config"
            config.write_text(f"ModulePath {options.mfl.parent}:{options.script.parent}\n"
                "Style * TileCascadePlacement\n"
                "AddToFunc StartFunction I Module FvwmMFL\n")
            process = runtime.start("core", [str(options.fvwm), "-d", display,
                "-f", str(config)], "core.log", runtime.environment({
                    "FVWMMFL_SOCKET_PATH": str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate("execution endpoint", endpoint.exists,
                time.monotonic() + 10, process=process)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b"set echo\n")
            connection.echo("execution-ready")

            def launch(name, body, settings=""):
                report = runtime.root / (name + "-report")
                script = runtime.root / name
                script.write_text(f"WindowTitle {{{name}}}\nWindowSize 90 40\n"
                    "Init\nBegin\n"
                    f" Set $file={{{report}}}\n Set $count=0\n"
                    " Set $counter=0\n Set $sent=0\n" + body +
                    " WriteToFile $file {completed}\nEnd\n"
                    "PeriodicTasks\nBegin\n If $sent==0 Then\n Begin\n"
                    " WriteToFile $file {recovered:} $count {:} $counter\n"
                    " Set $sent=1\n End\nEnd\n"
                    "Widget 1\nProperty\n Type ItemDraw\n Position 5 5\n"
                    " Size 60 20\n Title {budget}\nMain\n"
                    "Case message of\n1 :\nBegin\n Set $counter=0\n"
                    " For $counter=1 To 2147483647 Do\n Begin\n End\n"
                    " WriteToFile $file {for-unreachable}\nEnd\n"
                    "2 :\nBegin\n While 1==1 Do\n Begin\n End\n"
                    " WriteToFile $file {while-unreachable}\nEnd\n"
                    "3 :\nBegin\n Set $count=(Add $count 1)\n"
                    " SendSignal 1 3\n WriteToFile $file {recursive-unreachable}\nEnd\n"
                    "99 :\nBegin\n WriteToFile $file {event:} $count {:} $counter\nEnd\nEnd\n")
                shutil.copy2(script, options.artifacts_dir / (name + ".script"))
                connection.send((settings + "Module " + options.script.name +
                    " " + str(script) + "\n").encode())
                wait_for_predicate(name + " periodic recovery",
                    lambda: report.exists() and "recovered:" in report.read_text(),
                    time.monotonic() + 10, process=process)
                return report

            def signal_and_snapshot(report, message, expected):
                previous = report.read_text()
                connection.send((f"SendToModule FvwmScript SendString 1 {message}\n"
                    "SendToModule FvwmScript SendString 1 99\n").encode())
                wait_for_predicate("event after refusal",
                    lambda: report.read_text() != previous and
                        expected in report.read_text(),
                    time.monotonic() + 10, process=process)
                text = report.read_text()
                assert "unreachable" not in text, text
                return text

            def stop():
                connection.send(b"KillModule FvwmScript\n")
                connection.echo("script-killed")

            report = launch("DefaultBudget",
                " While 1==1 Do\n Begin\n Set $count=(Add $count 1)\n End\n")
            assert "recovered:4997:0\n" in report.read_text(), report.read_text()
            assert "completed" not in report.read_text()
            evidence["default-10000-and-periodic-recovery"] = "passed"
            signal_and_snapshot(report, 1, "event:4997:9998\n")
            evidence["empty-for-and-next-event"] = "passed"
            signal_and_snapshot(report, 2, "event:4997:9998\n")
            evidence["empty-while-and-next-event"] = "passed"
            signal_and_snapshot(report, 3, "event:5061:9998\n")
            evidence["default-depth-64-shared-signals"] = "passed"
            shutil.copy2(report, options.artifacts_dir / "default-report.txt")
            stop()

            report = launch("OverrideBudget",
                " For $counter=1 To 6000 Do\n Begin\n"
                " Set $count=(Add $count 1)\n End\n",
                "*FvwmScript: ExecutionLimit 20000\n"
                "*FvwmScript: RecursionLimit 4\n"
                "*FvwmScript: ExecutionLimit 0\n"
                "*FvwmScript: RecursionLimit 257\n")
            assert "completed\nrecovered:6000:6000\n" in report.read_text()
            signal_and_snapshot(report, 3, "event:6004:6000\n")
            evidence["overrides-and-invalid-values-retain-settings"] = "passed"
            shutil.copy2(report, options.artifacts_dir / "override-report.txt")
            stop()

            # Four setup instructions and this final write use exactly five.
            report = launch("ExactBudget", "", "*FvwmScript: ExecutionLimit 5\n"
                "*FvwmScript: RecursionLimit 64\n")
            assert "completed\nrecovered:0:0\n" in report.read_text()
            evidence["exact-limit-completion"] = "passed"
            shutil.copy2(report, options.artifacts_dir / "exact-report.txt")
            stop()
            log = (runtime.root / "core.log").read_text()
            assert log.count("handler stopped at ExecutionLimit") == 3, log
            assert log.count("handler stopped at RecursionLimit") == 2, log
            evidence["one-diagnostic-per-stopped-handler"] = "passed"
        finally:
            if connection:
                connection.close()
            for path in runtime.root.glob("*.log"):
                shutil.copy2(path, options.artifacts_dir / path.name)
            (options.artifacts_dir / "results.json").write_text(
                json.dumps(evidence, indent=2) + "\n")
    print("FvwmScript execution and nesting limits/recovery passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ("fvwm", "mfl", "script", "artifacts-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--display")
    args = parser.parse_args()
    for name in ("fvwm", "mfl", "script", "artifacts_dir"):
        setattr(args, name, getattr(args, name).resolve())
    run(args)
