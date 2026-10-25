iconman_sources = [
    'FvwmIconMan.c',
    'debug.c',
    'debug.h',
    'functions.c',
    'fvwm.c',
    'iconman_packet.c',
    'globals.c',
    'readconfig.c',
    'winlist.c',
    'x.c',
    'xmanager.c',
]

fvwm_iconman = executable(
    'FvwmIconMan',
    sources: iconman_sources,
    dependencies: all_compile_deps + text_runtime_deps,
    include_directories: includedirs,
    install_dir: fvwm_moduledir,
    install: true,
    link_with: libfvwm3,
)

install_data('ConfigFvwmIconMan', install_dir: fvwm_datadir)
