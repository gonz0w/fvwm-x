module_sources = [
    'FvwmButtons.c',
    'buttons_packet.c',
    'button.c',
    'draw.c',
    'dynamic.c',
    'icons.c',
    'misc.c',
    'output.c',
    'parse.c',
]

fvwm_buttons = executable(
    'FvwmButtons',
    module_sources,
    dependencies: all_found_deps,
    include_directories: includedirs,
    install_dir: fvwm_moduledir,
    install: true,
    link_with: libfvwm3,
)

install_data('ConfigFvwmButtons', install_dir: fvwm_datadir)
