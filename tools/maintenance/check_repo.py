#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check the local repository foundation; not a native/runtime acceptance gate."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
REQUIRED = (
    'README.md', 'CONTRIBUTING.md', 'GOVERNANCE.md', 'SECURITY.md', 'COPYING',
    'AGENTS.md', 'Makefile', 'dev-docs/PROJECT-TRACKER.md',
    '.github/CODEOWNERS', '.github/PULL_REQUEST_TEMPLATE.md',
    '.github/ISSUE_TEMPLATE/config.yml', '.github/ISSUE_TEMPLATE/bug.yml',
    '.github/ISSUE_TEMPLATE/work.yml', '.githooks/pre-commit',
)


def main():
    errors = []
    for name in REQUIRED:
        path = ROOT / name
        if not path.is_file():
            errors.append('missing foundation file: ' + name)
    workflows = ROOT / '.github/workflows'
    if workflows.is_dir() and any(workflows.glob('*.y*ml')):
        errors.append('active GitHub Actions files contradict local-only policy')
    readme = ROOT / 'README.md'
    if readme.is_file():
        text = readme.read_text()
        if len(text.split()) > 250:
            errors.append('keep README at most 250 words; detail belongs in guides')
        if '**Status:**' not in text:
            errors.append('README needs a brief current-status paragraph')
        if 'badge.svg' in text or 'api.codacy.com' in text:
            errors.append('README must not advertise inherited upstream checks')
    for name in ('README.md', 'CONTRIBUTING.md', 'GOVERNANCE.md', 'SECURITY.md'):
        path = ROOT / name
        if not path.is_file():
            continue
        text = path.read_text()
        if '\r' in text or not text.endswith('\n'):
            errors.append('expected LF and final newline: ' + name)
        for link in re.findall(r'\]\(([^)]+)\)', text):
            if '://' in link or link.startswith('#'):
                continue
            target = link.split('#', 1)[0]
            if target and not (path.parent / target).exists():
                errors.append('missing local link in ' + name + ': ' + target)
    if errors:
        for error in errors:
            print('Repository check: ' + error, file=sys.stderr)
        return 1
    print('Repository foundation passes; native/product checks are separate.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
