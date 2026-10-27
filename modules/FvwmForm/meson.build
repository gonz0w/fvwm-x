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

configure_file(
    input: 'FvwmTalk-wrapper.in',
    output: 'FvwmTalk',
    configuration: conf,
    install_dir: fvwm_moduledir,
    install_mode: install_mask_755,
)
