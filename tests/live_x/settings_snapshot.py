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
    groups = result.get('group_defaults', {})
    metrics = result.get('desktop_metric_defaults', {})
    assert isinstance(groups, dict) and isinstance(metrics, dict)
    value_version = result.get('value_inheritance_version', 0)
    if type(value_version) is not int or value_version not in (0, 1):
        raise ValueError('unsupported settings value inheritance')
    rows = {}
    for key, row in result['settings'].items():
        inherited = dict(shared, **defaults)
        if row.get('group') == 'application':
            inherited.update(application)
        group = row.get('group', inherited.get('group'))
        inherited.update(groups.get(group, {}))
        if (group == 'desktop' and 'component_version' in row and
                row['component_version'] == metrics.get('component_version')):
            inherited.update(metrics)
        expanded = dict(inherited, **row)
        if value_version == 1:
            if 'saved' not in row:
                expanded['saved'] = expanded['default']
            if 'effective' not in row:
                expanded['effective'] = expanded['saved']
            if 'resolution' in expanded and 'resolved' not in row:
                expanded['resolved'] = expanded['effective']
            if 'requested' not in row:
                if expanded.get('capability') == 'optional-tray-preference':
                    expanded['requested'] = expanded['effective']
                elif 'requested_source' in expanded:
                    expanded['requested'] = expanded['saved']
        if (expanded.get('source') == 'runtime-kept-preference-unsaved'
                and 'persistence_error' not in row):
            expanded['persistence_error'] = 'Runtime unsaved.'
        rows[key] = expanded
    result['settings'] = rows
    return result
