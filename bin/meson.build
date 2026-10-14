if golang.found()
    subdir('FvwmPrompt')
endif

if not golang.found()
    # The disabled-Go profile retains the legacy client until its own
    # default/minimal runtime migration checkpoint.
    FvwmCommand = configure_file(
        input: 'FvwmCommand.in',
        output: 'FvwmCommand',
        configuration: file_config,
        install: true,
        install_dir: bindir,
        install_mode: install_mask_755,
        install_tag: 'command-client',
    )
endif

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

if not golang.found()
    fvwm_convert = configure_file(
        input: 'fvwm-convert-2.6.in',
        output: 'fvwm-convert-2.6',
        configuration: file_config,
        install: true,
        install_dir: bindir,
        install_mode: install_mask_755,
        install_tag: 'config-converter',
    )
endif

if not golang.found()
    fvwm_menu_xlock = configure_file(
        input: 'fvwm-menu-xlock.in',
        output: 'fvwm-menu-xlock',
        configuration: file_config,
        install: true,
        install_dir: bindir,
        install_mode: install_mask_755,
        install_tag: 'xlock-menu',
    )
endif

if not golang.found()
    fvwm_menu_directory = configure_file(
        input: 'fvwm-menu-directory.in',
        output: 'fvwm-menu-directory',
        configuration: file_config,
        install: true,
        install_dir: bindir,
        install_mode: install_mask_755,
        install_tag: 'directory-menu',
    )
endif

if not golang.found()
    fvwm_menu_desktop = configure_file(
        input: 'fvwm-menu-desktop.in',
        output: 'fvwm-menu-desktop',
        configuration: file_config,
        install: true,
        install_dir: bindir,
        install_mode: install_mask_755,
        install_tag: 'desktop-menu',
    )

    config_data = install_data(
        'fvwm-menu-desktop-config.fpl',
        install_dir: fvwm_datadir,
        install_tag: 'desktop-menu',
    )
else
    configure_file(
        input: 'fvwm-menu-desktop-config.fpl.in',
        output: 'fvwm-menu-desktop-config.fpl',
        copy: true,
        install: true,
        install_dir: fvwm_datadir,
        install_tag: 'desktop-menu',
    )
endif

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
