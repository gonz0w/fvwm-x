libwidget_sources = [
    'script_delay.c',
    'script_geometry.c',
    'script_widget_math.c',
    'script_text.c',
    'Widgets/CheckBox.c',
    'Widgets/HDipstick.c',
    'Widgets/HScrollBar.c',
    'Widgets/ItemDraw.c',
    'Widgets/List.c',
    'Widgets/Menu.c',
    'Widgets/MiniScroll.c',
    'Widgets/PopupMenu.c',
    'Widgets/PushButton.c',
    'Widgets/RadioButton.c',
    'Widgets/Rectangle.c',
    'Widgets/Swallow.c',
    'Widgets/TextField.c',
    'Widgets/Tools.c',
    'Widgets/Tools.h',
    'Widgets/VDipstick.c',
    'Widgets/VScrollBar.c',
    'Widgets/Widget.c',
]

# Todo: could/should this be a shared library?
libWidgets = static_library(
    'Widgets',
    sources: libwidget_sources,
    dependencies: all_compile_deps,
    include_directories: includedirs,
    install_dir: get_option('libdir'),
    link_with: libfvwm3,
)

fvwmscript_sources = [
    'FvwmScript.c',
    'script_block.c',
    'Instructions.c',
    'script_file.c',
    'script_command.c',
    'script_output.c',
    'script_index.c',
    'script_process.c',
    'script_fifo.c',
    'script_value.c',
    'script_execution.c',
    'script_storage.c',
    'script_packet.c',
    'script_message.c',
    'libyywrap.c',
    'scanner.c',
    'script.c',
]

fvwm_script = executable(
    'FvwmScript',
    sources: fvwmscript_sources,
    dependencies: all_compile_deps + localized_text_runtime_deps +
        module_session_deps,
    include_directories: includedirs,
    install_dir: fvwm_moduledir,
    install: true,
    link_with: [libfvwm3, libWidgets],
)

install_scripts = [
    'Scripts/FvwmScript-BaseConfig',
    'Scripts/FvwmScript-BellSetup',
    'Scripts/FvwmScript-Buttons',
    'Scripts/FvwmScript-Colorset',
    'Scripts/FvwmScript-Date',
    'Scripts/FvwmScript-FileBrowser',
    'Scripts/FvwmScript-Find',
    'Scripts/FvwmScript-KeyboardSetup',
    'Scripts/FvwmScript-PointerSetup',
    'Scripts/FvwmScript-Quit',
    'Scripts/FvwmScript-ScreenDump',
    'Scripts/FvwmScript-ScreenSetup',
    'Scripts/FvwmScript-WidgetDemo',
]

install_data(
    install_scripts,
    install_dir: fvwm_datadir,
)

# Quote the installed native helper once; the communication ID stays literal.
com_arguments = [fvwm_datadir / 'fvwm-script-ComExample']
com_config = configuration_data()
com_config.set('COM_HELPER', run_command(py, '-c',
    'import shlex, sys; print(shlex.join(sys.argv[1:]))',
    com_arguments, check: true).stdout().strip())
configure_file(
    input: 'Scripts/FvwmScript-ComExample.in',
    output: 'FvwmScript-ComExample',
    configuration: com_config,
    install_dir: fvwm_datadir,
    install_tag: 'script-com-example',
)
