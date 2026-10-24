# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Invoke the installed/build native helper for independent review captures."""

import subprocess


class NativeCapture:
    def __init__(self, executable):
        self.executable = str(executable)

    def screenshot(self, directory):
        result = subprocess.run([self.executable, str(directory)], text=True,
            capture_output=True, timeout=35)
        if result.returncode != 0 or not result.stdout.startswith('Screenshot saved: '):
            raise RuntimeError(result.stderr or 'Native capture failed')
        return result.stdout.rstrip('\n')
