# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Resolve documented public settings metadata; standalone records need no merge."""

def expanded_settings(snapshot):
    result = dict(snapshot)
    if 'settings' not in result:
        return result
    shared = {key: result[key] for key in (
        'owner', 'generation', 'preview', 'persistence_error',
        'available', 'capability') if key in result}
    defaults = result.get('setting_defaults', {})
    application = result.get('application_defaults', {})
    assert isinstance(defaults, dict) and isinstance(application, dict)
    rows = {}
    for key, row in result['settings'].items():
        inherited = dict(shared, **defaults)
        if row.get('group') == 'application':
            inherited.update(application)
        expanded = dict(inherited, **row)
        if (expanded.get('source') == 'runtime-kept-preference-unsaved'
                and 'persistence_error' not in row):
            expanded['persistence_error'] = 'Runtime unsaved.'
        rows[key] = expanded
    result['settings'] = rows
    return result
