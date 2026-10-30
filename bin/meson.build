subdir('FvwmPrompt')

fvwm_root = executable(
    'fvwm-root',
    'fvwm-root.c',
    include_directories: includedirs,
    link_with: libfvwm3,
    dependencies: all_compile_deps + graphics_init_runtime_deps,
    install: true,
)

fvwm_perllib = configure_file(
    input: 'fvwm-perllib.in',
    output: 'fvwm-perllib',
    configuration: file_config,
    install: true,
    install_dir: bindir,
    install_mode: install_mask_755,
)

# The native provider owns settings; retain only the FvwmPerl entrypoint
# adapter until the separately tracked framework retirement (#42).
configure_file(
    input: 'fvwm-menu-desktop-config.fpl.in',
    output: 'fvwm-menu-desktop-config.fpl',
    copy: true,
    install: true,
    install_dir: fvwm_datadir,
    install_tag: 'desktop-menu',
)

# One private worker also serves software effects without optional SVG.
# A fresh executable avoids decoding in fork-inherited GLib/Rust state.
# ASan's shadow mapping cannot fit an enforced finite RLIMIT_AS; the parent
# and protocol stay instrumented, while this process keeps its real limit.
fvwm_svg_worker = executable(
    'fvwm-image-svg-worker',
    ['fvwm-image-svg-worker.c', '../libs/image_svg.c',
     '../libs/image_svg_protocol.c', '../libs/image_effect.c',
     '../libs/image_effect_protocol.c', '../libs/image_effect_x.c',
     '../libs/ImageSafety.c',
     '../libs/monitor_scale.c'],
    include_directories: includedirs,
    # The software effect path owns real X11 calls as well as renderer headers.
    dependencies: image_renderer_deps + [summary_depvals['x11']],
    override_options: ['b_sanitize=none'],
    install: true,
    install_dir: fvwm_moduledir,
)
worker_environment = environment()
worker_environment.set('FVWM_IMAGE_SVG_WORKER', fvwm_svg_worker.full_path())
worker_environment.set('FVWM_IMAGE_EFFECT_WORKER', fvwm_svg_worker.full_path())
add_test_setup('imageworker', env: worker_environment, is_default: true)

# The capture process owns its X connection and PNG writer. Link only its
# actual dependencies; disabled-PNG profiles install an explicit refusal stub.
capture_deps = [summary_depvals['x11']]
if libpng.found()
    capture_deps += libpng
endif
fvwm_desktop_capture = executable(
    'fvwm-desktop-capture',
    ['desktop_capture.c', '../libs/capture_policy.c', '../libs/ImageSafety.c'],
    include_directories: includedirs,
    dependencies: capture_deps,
    install: true,
    install_dir: fvwm_moduledir,
    install_tag: 'desktop-controller',
)

# One transient Xlib announcer is supervised before this wrapper execs the
# application in place. Go launchers retain pure metadata and literal argv.
fvwm_startup_launch = executable(
    'fvwm-startup-launch',
    ['startup_launch.c', '../libs/startup_notification_policy.c'],
    include_directories: includedirs,
    dependencies: [summary_depvals['x11']],
    install: true,
    install_dir: fvwm_moduledir,
    install_tag: 'desktop-controller',
)

# Default profiles install a dependency-free refusal stub. Only the optional
# transient worker links libdbus; the core and Go owner do not acquire it.
session_dbus = dependency('dbus-1', version: '>=1.14',
    required: get_option('session-dbus'))
session_bus_sources = ['session_bus_disabled.c']
session_bus_deps = []
if session_dbus.found()
    session_bus_sources = ['session_bus.c', '../libs/cJSON.c']
    session_bus_deps = [session_dbus]
endif
fvwm_session_bus = executable(
    'fvwm-session-bus', session_bus_sources,
    include_directories: includedirs,
    dependencies: session_bus_deps,
    install: true,
    install_dir: fvwm_moduledir,
    install_tag: 'session-startup',
)
