module_sources = [
    'FvwmBacker.c',
    'root_bits.c',
]

fvwm_backer = executable(
    'FvwmBacker',
    module_sources,
    dependencies: all_compile_deps + graphics_init_runtime_deps,
    include_directories: includedirs,
    install_dir: fvwm_moduledir,
    install: true,
    link_with: libfvwm3,
)

install_data('ConfigFvwmBacker', install_dir: fvwm_datadir)
