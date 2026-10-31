#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check proxy CLI dispatch without native modules, X or child processes."""
import builtins
from pathlib import Path
import runpy
import sys
import types
import unittest
from unittest.mock import patch


class ContrastFixtureCliTests(unittest.TestCase):
    def test_proxy_dispatch_precedes_fixture_imports_and_preserves_arguments(self):
        script = (Path(__file__).resolve().parent /
                  'live_x/contrast_resource_live.py')
        fixture_modules = {
            'geometry_transition_live', 'image_colorsets_live', 'colorset_live',
            'raise_unmanaged_live', 'image_owners_live', 'live_x_harness',
            'mfl_request_live', 'mfl_stream_live', 'session_x_errors',
            'browser_continuity_live', 'browser_handoff_live',
            'settings_preview_live', 'native_bindings_live'}
        calls, fixture_imports = [], []
        proxy = types.ModuleType('native_module_proxy')

        def module_proxy(real, directory, arguments, *, record_end_config=False):
            calls.append((real, directory, arguments, record_end_config))
            return 23

        proxy.module_proxy = module_proxy
        original_import = builtins.__import__

        def guarded_import(name, *args, **kwargs):
            if name.split('.')[0] in fixture_modules:
                fixture_imports.append(name)
                raise AssertionError('fixture imported before proxy: ' + name)
            return original_import(name, *args, **kwargs)

        arguments = ['17', '19', 'config with spaces', '42', 'unused']
        argv = [str(script), '--module-proxy', '/owned/real module',
                '/owned/records', *arguments]
        with patch.dict(sys.modules, native_module_proxy=proxy), patch.object(
                sys, 'argv', argv), patch.object(builtins, '__import__',
                guarded_import):
            with self.assertRaises(SystemExit) as stopped:
                runpy.run_path(str(script), run_name='__main__')
        self.assertEqual(stopped.exception.code, 23)
        self.assertEqual(calls, [('/owned/real module', '/owned/records',
                                 arguments, False)])
        self.assertEqual(fixture_imports, [])


if __name__ == '__main__':
    unittest.main()
