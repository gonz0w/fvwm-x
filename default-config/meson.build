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

# The remaining UI/settings controller uses Python; acquisition/actions use
# the adjacent private Go worker. Screenshot capture retains its native owner.
file_config.set('DESKTOP_PNG', libpng.found() ? 'true' : 'false')
fvwm_desktop = configure_file(
    input: 'desktop.py',
    output: 'FvwmDesktop',
    configuration: file_config,
    install: true,
    install_dir: fvwm_moduledir,
    install_mode: install_mask_755,
    install_tag: 'desktop-controller',
)

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
