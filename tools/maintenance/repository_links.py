#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check inline Markdown file links in current developer documentation, offline."""
import re
from pathlib import Path
from urllib.parse import unquote, urlsplit

from common import source_file


def markdown_links(text):
    """Skip code examples; handle angle targets, titles and balanced paths."""
    fence = None
    for line in text.splitlines():
        marker = re.match(r'^ {0,3}(`{3,}|~{3,})(.*)$', line)
        if fence:
            if (marker and marker[1][0] == fence[0] and
                    len(marker[1]) >= len(fence) and not marker[2].strip()):
                fence = None
            continue
        if marker:
            fence = marker[1]
            continue
        if line.startswith(('    ', '\t')):
            continue
        line = re.sub(r'(`+).*?\1', '', line)
        for match in re.finditer(r'!?\[[^\]\n]*\]\(\s*', line):
            start = match.end()
            if start < len(line) and line[start] == '<':
                end = line.find('>', start + 1)
                if end < 0:
                    continue
                target, end = line[start + 1:end], end + 1
            else:
                end, depth, target = start, 0, ''
                while end < len(line):
                    char = line[end]
                    if char == '\\' and end + 1 < len(line):
                        target += line[end + 1]
                        end += 2
                        continue
                    if char.isspace() or (char == ')' and depth == 0):
                        break
                    if char == '(':
                        depth += 1
                    elif char == ')':
                        depth -= 1
                    target += char
                    end += 1
            tail = line[end:].lstrip()
            if (tail.startswith(')') or
                    re.match(r'^(?:"[^"\n]*"|\'[^\'\n]*\')\s*\)', tail)):
                yield target


def documentation_paths(root):
    """Current guides/evidence only; archived plans and vendored docs are data."""
    root = Path(root)
    paths = set(root.glob('*.md'))
    paths.update(path for path in (root / 'dev-docs').rglob('*.md')
                 if 'history' not in path.relative_to(root / 'dev-docs').parts)
    for name in ('tools/maintenance/README.md', 'ci/README.md',
                 '.github/PULL_REQUEST_TEMPLATE.md', 'dev-docs/history/README.md'):
        path = root / name
        if path.is_file():
            paths.add(path)
    return sorted(paths)


def check_markdown_links(root, paths=None):
    """Check repository-local file existence; do not fetch URLs or check anchors."""
    root = Path(root).resolve()
    errors = []
    for path in documentation_paths(root) if paths is None else paths:
        path = Path(path)
        if not path.is_absolute():
            path = root / path
        try:
            # Normalize parent aliases (e.g. macOS /var -> /private/var)
            # without following the document: linked inputs are refused.
            path = path.parent.resolve() / path.name
            name = path.relative_to(root).as_posix()
            text = source_file(root, name).read_text()
        except (OSError, ValueError, RuntimeError) as error:
            errors.append('unreadable documentation: ' + str(error))
            continue
        for link in markdown_links(text):
            try:
                parsed = urlsplit(link)
                if parsed.scheme or parsed.netloc or not parsed.path:
                    continue
                target = (path.parent / unquote(parsed.path)).resolve()
                if target != root and root not in target.parents:
                    errors.append(f'local target escapes repository in {name}: {link}')
                elif not target.exists():
                    errors.append(f'missing local target in {name}: {link}')
            except (OSError, ValueError, RuntimeError) as error:
                errors.append(f'invalid local target in {name}: {link} ({error})')
    return errors
