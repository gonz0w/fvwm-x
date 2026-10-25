install_data(
    'ConfigFvwmPager',
    install_dir: join_paths(datadir, meson.project_name()),
)

fvwmpager_sources = [
    'fpmonitor.c',
    'FvwmPager.c',
    'FvwmPager.h',
    'init_pager.c',
    'messages.c',
    'pager_packet.c',
    'pager_geometry.c',
    'pager_shape.c',
    'pager_desk_range.c',
    'x_pager.c',
    'x_update.c',
]

fvwm_pager = executable(
    'FvwmPager',
    sources: fvwmpager_sources,
    dependencies: all_compile_deps + localized_text_runtime_deps,
    include_directories: includedirs,
    install_dir: fvwm_moduledir,
    install: true,
    link_with: libfvwm3,
)
