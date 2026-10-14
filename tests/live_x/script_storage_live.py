#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Check real value/storage boundaries and recovery without exhausting memory."""

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
    with HarnessRuntime(prefix="fvwm-script-storage-") as runtime:
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
            wait_for_predicate("storage endpoint", endpoint.exists,
                time.monotonic() + 10, process=process)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b"set echo\n")
            connection.echo("storage-ready")

            def launch(name, body, recovered, cases, settings=""):
                report = runtime.root / (name + "-report")
                script = runtime.root / name
                script.write_text(f"WindowTitle {{{name}}}\nWindowSize 90 40\n"
                    "Init\nBegin\n"
                    f" Set $file={{{report}}}\n Set $sent=0\n" + body +
                    " WriteToFile $file {completed}\nEnd\n"
                    "PeriodicTasks\nBegin\n If $sent==0 Then\n Begin\n"
                    " WriteToFile $file {recovered:} " + recovered + "\n"
                    " Set $sent=1\n End\nEnd\n"
                    "Widget 1\nProperty\n Type ItemDraw\n Position 5 5\n"
                    " Size 60 20\n Title {storage}\nMain\nCase message of\n" +
                    cases + "End\n")
                shutil.copy2(script, options.artifacts_dir / (name + ".script"))
                connection.send((settings + "Module " + options.script.name +
                    " " + str(script) + "\n").encode())
                wait_for_predicate(name + " periodic recovery",
                    lambda: report.exists() and "recovered:" in report.read_text(),
                    time.monotonic() + 10, process=process)
                return report

            def event(report, message, expected):
                previous = report.read_text()
                connection.send((f"SendToModule FvwmScript SendString 1 {message}\n").encode())
                wait_for_predicate("event after storage refusal",
                    lambda: report.read_text() != previous and expected in report.read_text(),
                    time.monotonic() + 10, process=process)
                assert "unreachable" not in report.read_text(), report.read_text()

            def stop(report=None):
                if report:
                    shutil.copy2(report, options.artifacts_dir / report.name)
                connection.send(b"KillModule FvwmScript\n")
                connection.echo("storage-module-killed")

            report = launch("DefaultValue",
                " Set $value={abcdefghijklmnop}\n"
                " For $i=1 To 16 Do\n Set $value=$value $value\n"
                " WriteToFile $file {exact:} (StrCopy $value 1048576 1048576)\n"
                " Set $value=$value {X}\n",
                "(StrCopy $value 1 16) (StrCopy $value 1048576 1048576)",
                "1 :\nBegin\n Set $value={small}\n"
                " WriteToFile $file {event:} $value\nEnd\n")
            text = report.read_text()
            assert "exact:p\nrecovered:abcdefghijklmnopp\n" in text, text
            assert "completed" not in text
            event(report, 1, "event:small\n")
            evidence["default-one-mib-exact-first-excess-old-value-and-event"] = "passed"
            stop(report)

            report = launch("SharedMemory",
                " Set $payload={abcdefghijklmnop}\n"
                " For $i=1 To 6 Do\n Set $payload=$payload $payload\n"
                " Set $x=$payload\n Set $y=$payload\n Set $z={prior}\n"
                " Set $z=$payload\n",
                "(StrCopy $x 1024 1024) (StrCopy $y 1024 1024) $z",
                "1 :\nBegin\n Set $x={}\n Set $z=$payload\n"
                " WriteToFile $file {event:} (StrCopy $z 1024 1024)\nEnd\n"
                "2 :\nBegin\n Set $x=$payload\n"
                " WriteToFile $file {unreachable}\nEnd\n"
                "99 :\nBegin\n WriteToFile $file {snapshot:} $x {:} "
                "(StrCopy $z 1024 1024)\nEnd\n",
                "*FvwmScript: ValueLimit 2048\n*FvwmScript: MemoryLimit 4096\n")
            text = report.read_text()
            assert "recovered:ppprior\n" in text and "completed" not in text, text
            event(report, 1, "event:p\n")
            for _ in range(3):
                connection.send(b"SendToModule FvwmScript SendString 1 2\n")
                event(report, 99, "snapshot::p\n")
            evidence["shared-total-preserved-value-shrink-and-repeated-recovery"] = "passed"
            stop(report)

            cases = "1 :\nBegin\n WriteToFile $file {event:} $value\nEnd\n"
            for name, expression in [("HexLimit", "(NumToHex 15 129)"),
                                     ("OutputLimit", "(GetOutput {printf '%0129d' 0} 1 -1)")]:
                report = launch(name, " Set $value={old}\n Set $value=" + expression + "\n",
                    "$value", cases,
                    "*FvwmScript: ValueLimit 128\n*FvwmScript: MemoryLimit 4096\n"
                    "*FvwmScript: ValueLimit 0\n*FvwmScript: MemoryLimit 0\n")
                assert "recovered:old\n" in report.read_text()
                assert "completed" not in report.read_text()
                event(report, 1, "event:old\n")
                stop(report)
            evidence["expanded-function-results-before-copy-and-invalid-settings"] = "passed"

            report = launch("LargerOverride",
                " Set $value=(GetOutput {printf '%0129d' 0} 1 -1)\n",
                "(StrCopy $value 129 129)", cases,
                "*FvwmScript: ValueLimit 256\n*FvwmScript: MemoryLimit 4096\n")
            assert "completed\nrecovered:0\n" in report.read_text()
            evidence["explicit-larger-value-override"] = "passed"
            stop(report)

            for name, settings, literal, expected in [
                ("ParseValueReject", "*FvwmScript: ValueLimit 128\n*FvwmScript: MemoryLimit 4096\n", "x"*129, "ValueLimit"),
                ("ParseMemoryReject", "*FvwmScript: ValueLimit 128\n*FvwmScript: MemoryLimit 8\n", "small", "MemoryLimit")]:
                script = runtime.root / name
                script.write_text(f"WindowTitle {{{name}}}\nWindowSize 90 40\n"
                    "Init\nBegin\n Set $value={" + literal + "}\nEnd\n"
                    "Widget 1\nProperty\n Type ItemDraw\n Position 5 5\n"
                    " Size 60 20\n Title {parser}\nEnd\n")
                shutil.copy2(script, options.artifacts_dir / (name + ".script"))
                connection.send((settings + "Module " + options.script.name +
                    " " + str(script) + "\n").encode())
                wait_for_predicate(name + " parser refusal",
                    lambda: any(str(script) in line and expected + " exceeded" in line
                        for line in (runtime.root / "core.log").read_text().splitlines()),
                    time.monotonic() + 10, process=process)
                stop()
            connection.echo("wm-after-parser-refusals")
            evidence["parser-value-and-total-refusal-and-wm-availability"] = "passed"
            log = (runtime.root / "core.log").read_text()
            assert log.count("handler stopped at ValueLimit") == 3, log
            assert log.count("handler stopped at MemoryLimit") == 4, log
            evidence["one-diagnostic-per-storage-refusal"] = "passed"
        finally:
            if connection:
                connection.close()
            for path in runtime.root.glob("*.log"):
                shutil.copy2(path, options.artifacts_dir / path.name)
            (options.artifacts_dir / "results.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("FvwmScript value/storage limits and recovery passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ("fvwm", "mfl", "script", "artifacts-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--display")
    args = parser.parse_args()
    for name in ("fvwm", "mfl", "script", "artifacts_dir"):
        setattr(args, name, getattr(args, name).resolve())
    run(args)
