executable(
    'FvwmRearrange',
    sources: 'FvwmRearrange.c',
    dependencies: all_compile_deps + [
        summary_depvals['x11'],
        summary_depvals['xrandr'],
    ],
    include_directories: includedirs,
    install_dir: fvwm_moduledir,
    install: true,
    link_with: libfvwm3,
)
