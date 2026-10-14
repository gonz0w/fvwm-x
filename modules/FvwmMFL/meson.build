fvwm_mfl = executable(
    'FvwmMFL',
    sources: ['FvwmMFL.c', 'stream.c'],
    dependencies: [all_found_deps],
    include_directories: includedirs,
    install_dir: fvwm_moduledir,
    install: true,
    link_with: libfvwm3,
)
