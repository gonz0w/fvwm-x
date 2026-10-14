#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Portable gettext maintenance; generate/update/compile into a fresh directory."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

from common import ROOT, fresh_output, source_file


def messages(text):
    # Match existing FVWM expansion and Script literal translation markers.
    # Unsupported escape syntax fails rather than silently changing a msgid.
    patterns = [r'\[gt\.((?:\\.|[^\]\\])*)\]',
                r'\b(?:WindowLocaleTitle|LocaleTitle)\s+\{((?:\\.|[^}\\])*)\}']
    found = []
    for pattern in patterns:
        for match in re.finditer(pattern, text):
            raw = match.group(1)
            value = json.loads('"' + raw.replace('\n', '\\n') + '"')
            if value:
                found.append((text.count('\n', 0, match.start()) + 1, value))
    return sorted(found)


def potfiles(source):
    paths = []
    for line in source_file(source, 'po/POTFILES').read_text().splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        name = line.strip("'")
        if name.startswith('../'):
            name = name[3:]
        source_file(source, name)
        paths.append(name)
    if not paths:
        raise ValueError('empty translation source list')
    return paths


def check_language(language):
    if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_@.-]*', language):
        raise ValueError('invalid language identifier')


def languages(source):
    """Use the same maintained catalog list as Meson's gettext target."""
    result = []
    for line in source_file(source, 'po/LINGUAS').read_text().splitlines():
        for language in line.split('#', 1)[0].split():
            check_language(language)
            if language in result:
                raise ValueError(f'duplicate language: {language}')
            source_file(source, f'po/{language}.po')
            result.append(language)
    if not result:
        raise ValueError('empty translation language list')
    return result


def catalog_operation(source, stage, action, language, template, use_fuzzy):
    if action == 'build':
        po = source_file(source, f'po/{language}.po')
        command = ['msgfmt', '--check', '--statistics']
        if use_fuzzy:
            command.append('--use-fuzzy')
        subprocess.run([*command, '-o', str(stage / f'fvwm3.{language}.gmo'), str(po)], check=True)
    elif action == 'init':
        if (source / 'po' / f'{language}.po').exists():
            raise ValueError('language exists; use update')
        subprocess.run(['msginit', '--no-translator', '-i', str(template),
                        '-l', language, '-o', str(stage / f'{language}.po')], check=True)
    else:
        po = source_file(source, f'po/{language}.po')
        subprocess.run(['msgmerge', '--no-fuzzy-matching', '-o',
                        str(stage / f'{language}.po'), str(po), str(template)], check=True)


def maintain(source, output, action, language=None, template=None, use_fuzzy=False,
             all_languages=False):
    source = Path(source).resolve()
    if action not in ('pot', 'build', 'init', 'update'):
        raise ValueError('unknown translation action')
    if all_languages:
        if language or action not in ('build', 'update'):
            raise ValueError('--all is only for build/update without --language')
        selected = languages(source)
    elif action != 'pot':
        if not language:
            raise ValueError('language is required')
        check_language(language)
        selected = [language]
    else:
        if language:
            check_language(language)
        selected = []
    if action in ('init', 'update'):
        template = Path(template).resolve() if template else source_file(source, 'po/fvwm3.pot')
    with fresh_output(output, source) as stage:
        if action == 'pot':
            paths = potfiles(source)
            native = [name for name in paths if name.endswith('.c')]
            candidate = stage / 'native.pot'
            subprocess.run(['xgettext', '--language=C', '--from-code=UTF-8',
                            '--add-comments=TRANSLATORS:', '--keyword=_', '--force-po',
                            '--copyright-holder=fvwm workers', '-o', str(candidate),
                            *native], cwd=source, check=True)
            with candidate.open('a') as stream:
                for name in paths:
                    if name in native:
                        continue
                    for line, message in messages(source_file(source, name).read_text()):
                        stream.write(f'\n#: {name}:{line}\nmsgid ' +
                                     json.dumps(message, ensure_ascii=False) + '\nmsgstr ""\n')
            subprocess.run(['msguniq', '--sort-output', '-o', str(stage / 'fvwm3.pot'),
                            str(candidate)], check=True)
            candidate.unlink()
        else:
            for selected_language in selected:
                catalog_operation(source, stage, action, selected_language,
                                  template, use_fuzzy)
        (stage / 'operation.json').write_text(json.dumps(
            {'action': action, 'language': language, 'languages': selected,
             'use_fuzzy': use_fuzzy}, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['pot', 'init', 'update', 'build', 'languages'])
    parser.add_argument('--source', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument('--language')
    selection.add_argument('--all', action='store_true', dest='all_languages')
    parser.add_argument('--template', type=Path)
    parser.add_argument('--use-fuzzy', action='store_true')
    args = parser.parse_args()
    if args.action == 'languages':
        if args.output or args.language or args.all_languages or args.template or args.use_fuzzy:
            parser.error('languages only accepts --source')
        print('\n'.join(languages(args.source)))
        return
    if args.output is None:
        parser.error('--output is required')
    if args.all_languages and args.action not in ('build', 'update'):
        parser.error('--all is only supported for build/update')
    if args.action != 'pot' and not (args.language or args.all_languages):
        parser.error('--language or --all is required')
    maintain(args.source, args.output, args.action, args.language,
             args.template, args.use_fuzzy, args.all_languages)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
