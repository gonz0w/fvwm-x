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
