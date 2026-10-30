fvwm_notify = executable(
    'FvwmNotify',
    ['notify.c', 'notify_model.c', 'notify_bus.c', 'notify_startup.c'],
    dependencies: all_compile_deps + localized_text_runtime_deps + [session_dbus],
    include_directories: includedirs,
    install_dir: fvwm_moduledir,
    install: true,
    install_tag: 'notifications',
    link_with: libfvwm3,
)
