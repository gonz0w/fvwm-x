# One manual manifest drives Meson and the standalone documentation renderer.
doc_tool = files('../tools/maintenance/docs.py')
# configure_file registers the manifest for automatic reconfiguration.
doc_manifest = configure_file(
    input: '../tools/maintenance/manuals.json',
    output: 'manuals.json',
    copy: true,
)
prompt_doc_args = golang.found() ? ['--prompt'] : []
manual_names = run_command(
    py, doc_tool, '--list', 'pages', prompt_doc_args,
    check: true,
).stdout().strip().split('\n')
adoc_sources = []
foreach name : manual_names
    adoc_sources += name + '.adoc'
endforeach

# The adoc files in the repo for fvwm3-commands, fvwm3-menus, and fvwm3-styles
# are generated from the main manpage source file. We need to extract the
# relevant sections from the main file to generate the source file includes.
fs = import('fs')
main_file = 'fvwm3_manpage_source.adoc'

# Custom build command for each source file
ad_build_targets = []
sections = run_command(
    py, doc_tool, '--list', 'sections', check: true,
).stdout().strip().split('\n')


foreach sec : sections
    outfile = f'fvwm3_@sec@.ad'
    # We could use a generator here, but since multiple targets include
    # the same file we would end up generating multiple times. Instead
    # Just build each section once and include it in the targets that need it.
    ad_build_targets += custom_target(
        output: outfile,
        input: main_file,
        command: [py, doc_tool, '--extract', sec, '--input', '@INPUT@'],
        capture: true,
        install: false,
    )
endforeach

# We need to copy the stub adoc files to the build directory
# so that we can generate docs from them; asciidoc includes
# the files by path, so we need to make sure they are in the
# same directory as the generated includes.
split_names = run_command(
    py, doc_tool, '--list', 'split_pages', check: true,
).stdout().strip().split('\n')
needs_generated_source = []
foreach name : split_names
    needs_generated_source += name + '.adoc'
endforeach

late_docs_targets = []
foreach src : needs_generated_source
    late_docs_targets += fs.copyfile(src)
endforeach

html_pages = []
if get_option('htmldoc')

    if get_option('docdir') != ''
        htmldir = get_option('docdir') / 'html'
    else
        htmldir = get_option('datadir') / 'doc' / meson.project_name() / 'html'
    endif

    index_src = 'index.adoc.in'
    index_dst = 'index.adoc'
    index = configure_file(
        input: index_src,
        output: index_dst,
        command: [py, doc_tool, '--index', '--input', '@INPUT@', prompt_doc_args],
        capture: true,
    )

    html_sources = adoc_sources

    foreach source : html_sources
        dest = source.replace('.adoc', '.html')
        doc_name_attribute = source.replace(
            '.adoc',
            '',
        )
        html_pages += custom_target(
            dest,
            input: source,
            output: dest,
            command: [
                asciidoc,
                '-b', 'html5',
                '-a', 'toc',
                '-a', 'webfonts!',
                '-a', doc_name_attribute,
                '-o', '@OUTPUT@',
                '@INPUT@',
            ],
            install: true,
            install_dir: htmldir,
        )
    endforeach

    foreach source : late_docs_targets
        # We can use the full_path method on a given custom_target to derive
        # the filename, then replace the extension to get our output.
        dest = source.full_path().split('/')[-1].replace('.adoc', '.html')
        doc_name_attribute = source.full_path().split('/')[-1].replace(
            '.adoc',
            '',
        )
        html_pages += custom_target(
            dest,
            input: source,
            output: dest,
            command: [
                asciidoc,
                '-b', 'html5',
                '-a', 'toc',
                '-a', 'webfonts!',
                '-a', doc_name_attribute,
                '-o', '@OUTPUT@',
                '@INPUT@',
            ],
            install: true,
            install_dir: htmldir,
            depends: ad_build_targets,
        )
    endforeach

    custom_target(
        'index.html',
        input: index,
        output: 'index.html',
        command: [
            asciidoc,
            '-b', 'html5',
            '-a', 'toc',
            '-a', 'webfonts!',
            '-a', 'index',
            '-o', '@OUTPUT@',
            '@INPUT@',
        ],
        install: true,
        install_dir: htmldir,
    )
endif

man_pages = []
if get_option('mandoc')
    # Each of the mans in the current makefile gets installed to man1
    foreach source : adoc_sources
        dest = source.replace('.adoc', '.1')
        doc_name_attribute = source.replace(
            '.adoc',
            '',
        )
        page_install = {}
        if doc_name_attribute == 'fvwm-file-browser'
            page_install = {'install_tag': 'file-browser'}
        endif
        man_pages += custom_target(
            dest,
            input: source,
            output: dest,
            command: [
                asciidoc,
                '-b', 'manpage',
                '-a', doc_name_attribute,
                '-o', '@OUTPUT@',
                '@INPUT@',
            ],
            install: true,
            install_dir: join_paths(get_option('mandir'), 'man1'),
            kwargs: page_install,
        )
    endforeach

    foreach source : late_docs_targets
        dest = source.full_path().split('/')[-1].replace('.adoc', '.1')
        doc_name_attribute = source.full_path().split('/')[-1].replace(
            '.adoc',
            '',
        )
        man_pages += custom_target(
            dest,
            input: source,
            output: dest,
            command: [
                asciidoc,
                '-b', 'manpage',
                '-a', doc_name_attribute,
                '-o', '@OUTPUT@',
                '@INPUT@',
            ],
            install: true,
            install_dir: join_paths(get_option('mandir'), 'man1'),
            depends: ad_build_targets,
        )
    endforeach
endif
