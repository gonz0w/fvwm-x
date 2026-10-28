fvwm_pinboard = executable(
    'FvwmPinboard',
    ['pinboard.c', 'pinboard_policy.c'],
    dependencies: all_compile_deps + localized_text_runtime_deps,
    include_directories: includedirs,
    install_dir: fvwm_moduledir,
    install: true,
    install_tag: 'pinboard',
    link_with: libfvwm3,
)
