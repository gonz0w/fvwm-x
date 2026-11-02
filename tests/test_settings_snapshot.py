#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Independent wire examples for settings metadata inheritance boundaries."""
import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent / 'live_x'))
from settings_snapshot import expanded_settings


class SettingsSnapshot(unittest.TestCase):
    def fixture(self):
        return dict(owner='FvwmDesktop', persistence_error='Common failure',
            setting_defaults=dict(group='tray', source='saved'),
            application_defaults=dict(readiness='ready'),
            group_defaults=dict(
                tray=dict(capability='optional-tray-preference', application='submitted'),
                desktop=dict(application='next-form')),
            desktop_metric_defaults=dict(component_version=1,
                component_roles=dict(menu_border=3)),
            settings=dict(
                tray_gap=dict(saved=4, effective=4),
                desktop_font=dict(group='desktop', component_version=1,
                    saved='fixed', effective='fixed'),
                desktop_icon_theme=dict(group='desktop', saved='', effective=''),
                future_metric=dict(group='desktop', component_version=2,
                    saved='inherit', effective='inherit'),
                application_font=dict(group='application', saved='', effective='')))

    def test_group_selection_and_version_boundary(self):
        wire = self.fixture()
        original = copy.deepcopy(wire)
        rows = expanded_settings(wire)['settings']
        self.assertEqual(rows['tray_gap']['capability'], 'optional-tray-preference')
        self.assertEqual(rows['desktop_font']['component_roles'],
            dict(menu_border=3))
        self.assertNotIn('component_roles', rows['desktop_icon_theme'])
        self.assertNotIn('component_roles', rows['future_metric'])
        self.assertEqual(rows['future_metric']['component_version'], 2)
        self.assertEqual(rows['application_font']['readiness'], 'ready')
        self.assertNotIn('capability', rows['application_font'])
        self.assertEqual(wire, original)

    def test_explicit_fields_and_null_error_win(self):
        wire = self.fixture()
        row = wire['settings']['desktop_font']
        row.update(application='not-submitted',
            component_roles=dict(menu_border=9),
            source='runtime-kept-preference-unsaved', persistence_error=None)
        expanded = expanded_settings(wire)['settings']['desktop_font']
        self.assertEqual(expanded['application'], 'not-submitted')
        self.assertEqual(expanded['component_roles'], dict(menu_border=9))
        self.assertIsNone(expanded['persistence_error'])
        del row['persistence_error']
        self.assertEqual(expanded_settings(wire)['settings']['desktop_font'][
            'persistence_error'], 'Runtime unsaved.')

    def test_missing_maps_and_unchanged_reply(self):
        wire = dict(owner='FvwmDesktop', settings=dict(
            font=dict(group='desktop', saved='fixed', effective='fixed')))
        self.assertEqual(expanded_settings(wire)['settings']['font'],
            dict(owner='FvwmDesktop', group='desktop', saved='fixed',
                effective='fixed'))
        unchanged = dict(state='unchanged', generation='opaque')
        self.assertEqual(expanded_settings(unchanged), unchanged)

    def test_equal_values_and_explicit_null_are_distinct(self):
        wire = self.fixture()
        wire['value_inheritance_version'] = 1
        font = wire['settings']['desktop_font']
        del font['effective']
        font['resolution'] = 'explicit'
        wire['settings']['tray_gap']['default'] = 4
        del wire['settings']['tray_gap']['saved']
        wire['settings']['tray_gap']['requested'] = None
        wire['settings']['session_bus'] = dict(group='session', saved='off',
            effective=None, requested_source='saved')
        rows = expanded_settings(wire)['settings']
        self.assertEqual(rows['desktop_font']['effective'], 'fixed')
        self.assertEqual(rows['desktop_font']['resolved'], 'fixed')
        self.assertEqual(rows['tray_gap']['saved'], 4)
        self.assertIsNone(rows['tray_gap']['requested'])
        self.assertIsNone(rows['session_bus']['effective'])
        self.assertEqual(rows['session_bus']['requested'], 'off')
        del wire['settings']['tray_gap']['requested']
        self.assertEqual(expanded_settings(wire)['settings']['tray_gap'][
            'requested'], 4)

    def test_unsupported_value_inheritance_fails_explicitly(self):
        for version in (True, 2, '1'):
            wire = self.fixture()
            wire['value_inheritance_version'] = version
            with self.assertRaises(ValueError):
                expanded_settings(wire)


if __name__ == '__main__':
    unittest.main()
