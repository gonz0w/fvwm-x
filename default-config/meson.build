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

# Artwork selection is independent of the optional SVG renderer. A minimal
# build can install the opt-in example while retaining labeled text fallback.
if get_option('lucide-menu-icons')
    lucide_theme = 'icons/FVWM-X-Lucide'
    lucide_install_dir = datadir / lucide_theme
    lucide_icons = [
        'folder', 'folder-open', 'file', 'file-text', 'image', 'clipboard',
        'copy', 'scissors', 'clipboard-paste', 'search', 'refresh-cw',
        'arrow-left', 'arrow-right', 'arrow-up', 'settings', 'trash',
    ]
    foreach name : lucide_icons
        install_data(
            lucide_theme / 'scalable/actions' / (name + '.svg'),
            install_dir: lucide_install_dir / 'scalable/actions',
            install_tag: 'lucide-menu-icons',
        )
    endforeach
    install_data(
        lucide_theme / 'index.theme', lucide_theme / 'LICENSE',
        lucide_theme / 'provenance.json', install_dir: lucide_install_dir,
        install_tag: 'lucide-menu-icons',
    )
    lucide_config = configuration_data()
    lucide_config.set('FVWM_LUCIDE_ICONDIR',
        prefix / lucide_install_dir / 'scalable/actions')
    lucide_menu_example = configure_file(
        input: 'lucide-menus.fvwm.in', output: 'lucide-menus.fvwm',
        configuration: lucide_config, install: true,
        install_dir: default_confdir, install_tag: 'lucide-menu-icons',
    )
endif
