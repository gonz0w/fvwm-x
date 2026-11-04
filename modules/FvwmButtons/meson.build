module_sources = [
    'FvwmButtons.c',
    'buttons_packet.c',
    'tray_container.c',
    'buttons_startup.c',
    'buttons_navigation.c',
    'buttons_updates.c',
    'keyboard.c',
    'button.c',
    'draw.c',
    'dynamic.c',
    'icons.c',
    'misc.c',
    'output.c',
    'parse.c',
]

buttons_dependencies = all_compile_deps + text_runtime_deps + module_session_deps
if xfixes.found()
    buttons_dependencies += [xfixes]
endif
fvwm_buttons = executable(
    'FvwmButtons',
    module_sources,
    dependencies: buttons_dependencies,
    include_directories: includedirs,
    install_dir: fvwm_moduledir,
    install: true,
    link_with: libfvwm3,
)

install_data('ConfigFvwmButtons', install_dir: fvwm_datadir)

if graphics_eval_enabled
    foreach backend : ['native', 'cairo']
        executable(
            'FvwmButtons-' + backend + '-eval',
            module_sources + ['buttons_graphics_eval.c'],
            dependencies: buttons_dependencies +
                (backend == 'cairo' ? [graphics_eval_cairo] : []),
            include_directories: includedirs,
            c_args: ['-DFVWM_GRAPHICS_EVAL=1'],
            install: false,
            link_with: [libfvwm3, graphics_eval_libraries[backend]],
        )
    endforeach
endif
