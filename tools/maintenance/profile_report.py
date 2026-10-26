#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Read normalized R1 receipts; propose empirical envelopes without acceptance.

Context strings identify exact manifests, including environment/kernel/geometry
and sampling/repeat policy. Revisions and artifact bytes may differ; scopes may
not. Run IDs assert independent observations; this helper cannot prove sampling.
Missing/null values are unavailable. Only complete coverage of every repetition
produces an envelope. A baseline is a frozen raw receipt, never candidate data.
"""
import argparse
import json
import math
from pathlib import Path
import sys

MAX_BYTES = 1024 * 1024
CONTEXT = {'profile', 'workload', 'workload_version', 'config', 'source_scope',
           'artifact_scope', 'platform', 'compiler', 'instrumentation',
           'effective_features'}
POLICY = ('Proposed empirical reference policy: max + max(range, quantum); '
          'no statistical confidence and no automatic acceptance.')


def fields(value, expected, label):
    if not isinstance(value, dict) or set(value) != expected:
        raise ValueError(label + ': unexpected or missing fields')


def identity(value):
    if not isinstance(value, str) or not value.strip() or len(value) > 1024:
        raise ValueError('identity must be a nonempty string of <=1024 characters')


def number(value):
    if type(value) not in (int, float):
        raise ValueError('metric/quantum must be a finite nonnegative number')
    try:
        valid = math.isfinite(value) and value >= 0
    except OverflowError:
        valid = False
    if not valid:
        raise ValueError('metric/quantum must be a finite nonnegative number')


def validate(receipt):
    fields(receipt, {'schema_version', 'context', 'source_revision',
                     'artifact_identity', 'metrics', 'runs'}, 'receipt')
    if type(receipt['schema_version']) is not int or receipt['schema_version'] != 1:
        raise ValueError('unsupported schema_version')
    context = receipt['context']
    fields(context, CONTEXT, 'context')
    for key in CONTEXT - {'effective_features'}:
        identity(context[key])
    features = context['effective_features']
    if not isinstance(features, dict) or not 1 <= len(features) <= 128:
        raise ValueError('effective_features must name 1..128 observed features')
    for name, state in features.items():
        identity(name)
        if state not in ('enabled', 'disabled', 'unavailable'):
            raise ValueError('effective feature needs enabled/disabled/unavailable')
    identity(receipt['source_revision'])
    identity(receipt['artifact_identity'])
    metrics = receipt['metrics']
    if not isinstance(metrics, dict) or not 1 <= len(metrics) <= 128:
        raise ValueError('metrics must name 1..128 metrics')
    for name, spec in metrics.items():
        identity(name)
        fields(spec, {'unit', 'scope', 'coverage', 'quantum'}, 'metric')
        identity(spec['unit'])
        identity(spec['scope'])
        if spec['coverage'] not in ('complete', 'partial', 'unavailable'):
            raise ValueError('coverage must be complete/partial/unavailable')
        number(spec['quantum'])
    runs = receipt['runs']
    if not isinstance(runs, list) or not 3 <= len(runs) <= 1000:
        raise ValueError('receipt needs 3..1000 independent runs')
    seen = set()
    for run in runs:
        fields(run, {'id', 'metrics'}, 'run')
        identity(run['id'])
        if run['id'] in seen:
            raise ValueError('duplicate run ID')
        seen.add(run['id'])
        values = run['metrics']
        if not isinstance(values, dict) or not set(values) <= set(metrics):
            raise ValueError('run has unknown metric or malformed metrics')
        for value in values.values():
            if value is not None:
                number(value)


def summarize(receipt):
    result = {key: receipt[key] for key in
              ('context', 'source_revision', 'artifact_identity')}
    result['run_ids'] = [run['id'] for run in receipt['runs']]
    result['metrics'] = {}
    for name, spec in receipt['metrics'].items():
        values = [run['metrics'].get(name) for run in receipt['runs']]
        available = [value for value in values if value is not None]
        complete = (len(available) == len(values) and
                    spec['coverage'] == 'complete')
        maximum = max(available) if available else None
        envelope = None
        if complete:
            envelope = maximum + max(maximum - min(available), spec['quantum'])
            number(envelope)
        result['metrics'][name] = dict(
            spec, available_runs=len(available), total_runs=len(values),
            observed_max=maximum, proposed_envelope=envelope)
    return result


def report(candidate, baseline=None):
    validate(candidate)
    if baseline is not None:
        validate(baseline)
        if candidate['context'] != baseline['context']:
            raise ValueError('context mismatch; comparison refused')
        if candidate['metrics'] != baseline['metrics']:
            raise ValueError('metric mismatch; comparison refused')
    result = {'schema_version': 1, 'policy': POLICY,
              'candidate': summarize(candidate)}
    if baseline is not None:
        result['baseline'] = summarize(baseline)
        result['comparison'] = {}
        for name, metric in result['candidate']['metrics'].items():
            reference = result['baseline']['metrics'][name]['proposed_envelope']
            if metric['proposed_envelope'] is None or reference is None:
                status = 'unavailable'
            elif metric['observed_max'] <= reference:
                status = 'within_proposed_envelope'
            else:
                status = 'above_proposed_envelope'
            result['comparison'][name] = status
    return result


def unique_members(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate JSON member: ' + key)
        result[key] = value
    return result


def load_receipt(path):
    with Path(path).open('rb') as stream:
        data = stream.read(MAX_BYTES + 1)
    if len(data) > MAX_BYTES:
        raise ValueError('receipt too large (maximum 1 MiB)')
    receipt = json.loads(data, object_pairs_hook=unique_members)
    validate(receipt)
    return receipt


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('receipt', type=Path)
    parser.add_argument('--baseline', type=Path,
                        help='frozen normalized raw receipt with matching context')
    args = parser.parse_args(argv)
    try:
        candidate = load_receipt(args.receipt)
        baseline = load_receipt(args.baseline) if args.baseline else None
        print(json.dumps(report(candidate, baseline), indent=2, allow_nan=False))
    except (OSError, ValueError, RecursionError) as error:
        print('profile_report: ' + str(error), file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
