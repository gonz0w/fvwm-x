#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Render shared manual manifest in a fresh directory, never inside doc/."""
import argparse
import html
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from common import ROOT, fresh_output, source_file


def section(text, name):
    start = f"// BEGIN '{name}'"
    end = f"// END '{name}'"
    lines = text.splitlines(keepends=True)
    starts = [i for i, line in enumerate(lines) if line.rstrip() == start]
    ends = [i for i, line in enumerate(lines) if line.rstrip() == end]
    if len(starts) != 1 or len(ends) != 1 or starts[0] >= ends[0]:
        raise ValueError(f'missing, duplicate, or reversed section: {name}')
    # Preserve nested section markers exactly as the existing extractor does.
    return ''.join(lines[starts[0] + 1:ends[0]])


def manual_manifest():
    return json.loads((Path(__file__).parent / 'manuals.json').read_text())


def index_source(text, prompt):
    if not prompt:
        text = ''.join(line for line in text.splitlines(keepends=True)
                       if 'xref:FvwmPrompt.adoc[' not in line)
    if 'xref:FvwmDesktop.adoc[' not in text:
        text += '\n== Default desktop\n\n* xref:FvwmDesktop.adoc[Slate desktop]\n'
    return text


def website_fragment(body, name, version):
    body = '\n'.join(line for line in body.splitlines() if 'toctitle' not in line)
    body = body.replace('id="toc" class="toc"', 'id="markdown-toc"')
    if name == 'index':
        body = body.replace('.html"', '/"').replace(' class="sect1"', '')
        body = body.replace('<p>', '').replace('</p>', '')
        header = ('---\ntitle: Fvwm3 Manual Pages\nshowtitle: 1\n---\n'
                  'The following are the manual pages for fvwm3 version '
                  '<strong>' + html.escape(version) + '</strong>.\n')
    else:
        body = body.replace('literalblock', 'literalblock highlight')
        header = (f'---\ntitle: {name} manual page\nshowtitle: 1\n'
                  f'permalink: /Man/{name}/index.html\n---\n')
    return header + body + '\n'


def render(source, output, formats, prompt, executable, website=False):
    docroot = Path(source) / 'doc'
    manifest = manual_manifest()
    pages = manifest['pages'] + manifest['split_pages']
    version = None
    if website:
        version = subprocess.check_output(
            ['sh', str(source_file(source, 'utils/fvwm-version-str.sh'))],
            cwd=source, text=True).strip()
        if not version:
            raise ValueError('could not determine source version')
    if prompt:
        pages += ['FvwmPrompt']
    with fresh_output(output, source) as stage:
        inputs = stage / 'sources'
        inputs.mkdir()
        # Copy all support includes, never modify the working documentation.
        for name in pages + ['fvwm3_manpage_source']:
            content = source_file(docroot, name + '.adoc').read_bytes()
            (inputs / (name + '.adoc')).write_bytes(content)
        main = (inputs / 'fvwm3_manpage_source.adoc').read_text()
        for name in manifest['sections']:
            (inputs / f'fvwm3_{name}.ad').write_text(section(main, name))
        index = index_source(source_file(docroot, 'index.adoc.in').read_text(), prompt)
        (inputs / 'index.adoc').write_text(index)
        for fmt in formats:
            destination = stage / fmt
            destination.mkdir()
            backend, extension = ('html5', '.html') if fmt == 'html' else ('manpage', '.1')
            for name in pages + (['index'] if fmt == 'html' else []):
                target = destination / (name + extension)
                command = [executable, '-b', backend, '-a', 'reproducible',
                           '-a', name, '-o', str(target)]
                if fmt == 'html':
                    command += ['-a', 'toc', '-a', 'webfonts!']
                if website:
                    command += ['-e']
                command.append(str(inputs / (name + '.adoc')))
                subprocess.run(command, check=True)
                if not target.is_file() or target.stat().st_size == 0:
                    raise ValueError(f'renderer produced no output: {name}')
                if website:
                    target.write_text(website_fragment(target.read_text(), name, version))
        (stage / 'manifest.json').write_text(json.dumps(
            {'pages': pages, 'formats': formats, 'prompt': prompt,
             'website': website}, indent=2) + '\n')


def website_directory(source, output, prompt, executable):
    """Prepare a flat Man/ directory without writing into source or replacing data."""
    with fresh_output(output, source) as stage:
        with tempfile.TemporaryDirectory(prefix='fvwm-website-') as temporary:
            rendered = Path(temporary) / 'rendered'
            render(source, rendered, ['html'], prompt, executable, website=True)
            for page in (rendered / 'html').glob('*.html'):
                shutil.copyfile(page, stage / page.name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--format', choices=['html', 'man', 'both'], default='both')
    parser.add_argument('--prompt', action='store_true')
    parser.add_argument('--website', action='store_true')
    parser.add_argument('--website-directory', action='store_true',
                        help='render website pages directly into a fresh output directory')
    parser.add_argument('--asciidoctor', default='asciidoctor')
    parser.add_argument('--list', choices=['pages', 'split_pages', 'sections'])
    parser.add_argument('--extract', choices=['commands', 'menus', 'styles'])
    parser.add_argument('--index', action='store_true')
    parser.add_argument('--input', type=Path)
    args = parser.parse_args()
    operations = sum(bool(value) for value in
                     (args.list, args.extract, args.index, args.output))
    if operations != 1:
        parser.error('select one of --output, --list, --extract or --index')
    if args.list:
        names = manual_manifest()[args.list]
        if args.list == 'pages' and args.prompt:
            names += ['FvwmPrompt']
        print('\n'.join(names))
        return
    if args.extract or args.index:
        if args.input is None:
            parser.error('--extract and --index require --input')
        text = args.input.read_text()
        sys.stdout.write(section(text, args.extract) if args.extract
                         else index_source(text, args.prompt))
        return
    if args.website and args.format != 'html':
        parser.error('--website requires --format html')
    if args.website_directory:
        if not args.website:
            parser.error('--website-directory requires --website')
        website_directory(args.source, args.output, args.prompt, args.asciidoctor)
        return
    render(args.source, args.output, ['html', 'man'] if args.format == 'both'
           else [args.format], args.prompt, args.asciidoctor, args.website)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
