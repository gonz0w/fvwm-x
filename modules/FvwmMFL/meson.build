fvwm_mfl = executable(
    'FvwmMFL',
    sources: ['FvwmMFL.c', 'stream.c', 'request.c'],
    dependencies: all_compile_deps + [summary_depvals['libevent']],
    include_directories: includedirs,
    install_dir: fvwm_moduledir,
    install: true,
    link_with: libfvwm3,
)
