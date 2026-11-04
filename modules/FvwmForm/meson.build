install_data = [
    'FvwmForm-Form',
    'FvwmForm-Capture',
    'FvwmForm-Desktop',
    'FvwmForm-QuitVerify',
    'FvwmForm-Rlogin',
    'FvwmForm-RootCursor',
    'FvwmForm-Talk',
    'FvwmForm-TalkHelp',
    'FvwmForm-XDGOptionsHelp',
    'FvwmForm-XDGMenuHelp',
]

install_data(
    install_data,
    install_dir: join_paths(get_option('datadir'), meson.project_name()),
)

fvwmform_sources = [
    'FvwmForm.c',
    'form_input.c',
    'file_transfer_form.c',
    'form_list.c',
    'form_scale.c',
    'form_scale_policy.c',
    'list_widget.c',
    'ReadXServer.c',
    'ParseCommand.c',
]

fvwm_form = executable(
    'FvwmForm',
    fvwmform_sources,
    dependencies: all_compile_deps + localized_text_runtime_deps + [
        summary_depvals['libevent'],
    ],
    include_directories: includedirs,
    install_dir: fvwm_moduledir,
    install: true,
    link_with: libfvwm3,
)

if graphics_eval_enabled
    foreach backend : ['native', 'cairo']
        executable(
            'FvwmForm-' + backend + '-eval',
            fvwmform_sources + ['form_graphics_eval.c'],
            dependencies: all_compile_deps + localized_text_runtime_deps +
                [summary_depvals['libevent']] +
                (backend == 'cairo' ? [graphics_eval_cairo] : []),
            include_directories: includedirs,
            c_args: ['-DFVWM_GRAPHICS_EVAL=1'],
            install: false,
            link_with: [libfvwm3, graphics_eval_libraries[backend]],
        )
    endforeach
endif

configure_file(
    input: 'FvwmTalk-wrapper.in',
    output: 'FvwmTalk',
    configuration: conf,
    install_dir: fvwm_moduledir,
    install_mode: install_mask_755,
)
