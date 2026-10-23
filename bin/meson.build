subdir('FvwmPrompt')

# We should tighten up the dependencies here, but for now we'll just use all_found_deps.
fvwm_root = executable(
    'fvwm-root',
    'fvwm-root.c',
    include_directories: includedirs,
    link_with: libfvwm3,
    dependencies: all_found_deps,
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
    dependencies: all_found_deps,
    override_options: ['b_sanitize=none'],
    install: true,
    install_dir: fvwm_moduledir,
)
worker_environment = environment()
worker_environment.set('FVWM_IMAGE_SVG_WORKER', fvwm_svg_worker.full_path())
worker_environment.set('FVWM_IMAGE_EFFECT_WORKER', fvwm_svg_worker.full_path())
add_test_setup('imageworker', env: worker_environment, is_default: true)
