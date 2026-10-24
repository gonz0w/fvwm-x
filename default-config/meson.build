default_confdir = join_paths(
    get_option('datadir'),
    meson.project_name(),
    'default-config',
)

default_conf_files = [
    'config',
    'desktop.fvwm',
    'palettes.json',
    'fvwm-x.png',
]

# Native capture stays separately supervised until its reviewed Xlib port.
fvwm_desktop_capture = configure_file(
    input: 'capture.py',
    output: 'fvwm-desktop-capture',
    configuration: file_config,
    install: true,
    install_dir: fvwm_moduledir,
    install_mode: install_mask_755,
    install_tag: 'desktop-controller',
)

# The uninstalled Go module executes this configured helper too. Reused
# configure_file output may retain an older mode when its bytes are equal.
run_command('chmod', '0755', fvwm_desktop_capture, check: true)

foreach palette : ['slate', 'dune', 'grove', 'ink']
    foreach appearance : ['light', 'dark']
        default_conf_files += 'wallpapers' / (palette + '-' + appearance + '.png')
    endforeach
endforeach

install_data(
    default_conf_files,
    install_dir: default_confdir,
    preserve_path: true,
)
