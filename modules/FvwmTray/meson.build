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

# Optional Linux-only observation binaries. Production sources/flags and install
# outputs above are unchanged. Wrapping is not a portable runtime dependency.
if (get_option('live-x-tests') and host_machine.system() == 'linux'
    and session_dbus.found())
    tray_icon_probe_wrap = [
        '-Wl,--wrap=tray_icons_ready',
        '-Wl,--wrap=tray_icons_busy',
        '-Wl,--wrap=tray_icons_timeout_ms',
        '-Wl,--wrap=tray_bus_model',
        '-Wl,--wrap=tray_sni_sync',
        '-Wl,--wrap=PCacheFvwmPictureAtScale',
    ]
    tray_icon_probe_cc = meson.get_compiler('c')
    tray_icon_probe_supported = tray_icon_probe_cc.get_supported_link_arguments(
        tray_icon_probe_wrap,
    )
    if tray_icon_probe_supported.length() == tray_icon_probe_wrap.length()
        tray_icon_probe_source = files('../../tests/tray_icon_probe.c')
        tray_icon_probe_python = find_program('python3')
        tray_icon_probe_control = custom_target(
            'tray-icon-control-source',
            input: 'tray_sni_ui.c',
            output: 'tray_sni_ui_control.c',
            command: [tray_icon_probe_python,
                files('../../tests/tray_icon_control_source.py'),
                '@INPUT@', '@OUTPUT@'],
        )
        tray_icon_control_sources = []
        foreach source : tray_sources
            if source != 'tray_sni_ui.c'
                tray_icon_control_sources += [source]
            endif
        endforeach
        executable(
            'FvwmTrayIconProbe',
            tray_sources, tray_icon_probe_source,
            dependencies: tray_dependencies,
            include_directories: includedirs,
            link_with: libfvwm3,
            link_args: tray_icon_probe_wrap,
            override_options: ['b_lto=false'],
            install: false,
        )
        executable(
            'FvwmTrayIconControl',
            tray_icon_control_sources,
            tray_icon_probe_control,
            tray_icon_probe_source,
            dependencies: tray_dependencies,
            include_directories: includedirs,
            link_with: libfvwm3,
            link_args: tray_icon_probe_wrap,
            override_options: ['b_lto=false'],
            install: false,
        )
    else
        message('Tray icon observer unavailable: linker wrapping unsupported')
    endif
endif
