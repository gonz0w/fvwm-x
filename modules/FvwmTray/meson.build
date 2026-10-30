fvwm_tray = executable(
    'FvwmTray',
    ['tray.c', 'tray_x11.c', 'tray_model.c'],
    dependencies: all_compile_deps + localized_text_runtime_deps,
    include_directories: includedirs,
    install_dir: fvwm_moduledir,
    install: true,
    install_tag: 'tray',
    link_with: libfvwm3,
)
