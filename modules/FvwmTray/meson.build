tray_sources = [
    'tray.c', 'tray_x11.c', 'tray_model.c', 'tray_container.c',
    'tray_sni_model.c', 'tray_sni_ui.c', 'tray_icons.c',
]
tray_dependencies = all_compile_deps + localized_text_runtime_deps
if session_dbus.found()
    tray_sources += ['tray_bus.c']
    tray_dependencies += [session_dbus]
else
    tray_sources += ['tray_bus_disabled.c']
endif
if xfixes.found()
    tray_dependencies += [xfixes]
endif
fvwm_tray = executable(
    'FvwmTray',
    tray_sources,
    dependencies: tray_dependencies,
    include_directories: includedirs,
    install_dir: fvwm_moduledir,
    install: true,
    install_tag: 'tray',
    link_with: libfvwm3,
)
