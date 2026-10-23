# Fvwmprompt is written in golang. Unfortunately there is still
# no automagic in meson to handle golang projects. So we have to
# do it manually.

# Extract all of the go sources from the current directory
r = run_command('find-go-sources.sh', meson.current_source_dir(), check: true)
fvwmprompt_files = r.stdout().strip().split()
fvwmprompt_files += 'go.mod'
fvwmprompt_files += 'go.sum'

fvwmprompt_build = meson.current_source_dir()
fvwmprompt_output = meson.current_build_dir() + '/FvwmPrompt'
# Keep SDK compilation job-local and reuse it for the native tests.
fvwmprompt_go_cache = meson.current_build_dir() / 'go-cache'

fvwmprompt = custom_target(
    'FvwmPrompt',
    output: 'FvwmPrompt',
    input: fvwmprompt_files,
    command: [
        'env', 'GOCACHE=' + fvwmprompt_go_cache,
        'GOTOOLCHAIN=local', 'GOPROXY=off', 'GOSUMDB=off',
        golang,
        'build',
        '-C', fvwmprompt_build,
        '-v',
        '-mod=vendor',
        '-o', fvwmprompt_output,
    ],
    install: true,
    install_dir: bindir,
    install_tag: 'prompt-client',
)

# Compile test-only SDK dependencies during the build, before the runtime
# test deadline. These binaries are private build outputs, never installed.
fvwmprompt_test_binaries = custom_target(
    'FvwmPrompt-test-binaries',
    output: [
        'FvwmPrompt.test', 'fvwmcommand.test',
        'fvwmmenudesktop.test', 'fvwmmenudirectory.test',
        'fvwmmenuxlock.test', 'fvwmscriptcom.test', 'xdgmenu.test',
        'xdgsource.test', 'transport.test',
        'fvwmfilebrowser.test', 'collection.test', 'filemodel.test',
    ],
    input: fvwmprompt_files,
    command: [
        'env', 'GOCACHE=' + fvwmprompt_go_cache,
        'GOTOOLCHAIN=local', 'GOPROXY=off', 'GOSUMDB=off',
        golang, 'test', '-C', fvwmprompt_build, '-mod=vendor',
        '-c', '-o', meson.current_build_dir() + '/', './...',
    ],
    build_by_default: false,
)

prompt_test_env = environment()
prompt_test_env.set('GOCACHE', fvwmprompt_go_cache)
prompt_test_env.set('GOTOOLCHAIN', 'local')
prompt_test_env.set('GOPROXY', 'off')
test('prompt-transport', golang,
    args: ['test', '-C', fvwmprompt_build, '-mod=vendor', './...'],
    depends: fvwmprompt_test_binaries,
    env: prompt_test_env,
    timeout: 180,
)
test('prompt-client', py,
    args: [files('../../tests/test_prompt_client.py'), '--prompt', fvwmprompt],
    timeout: 60,
)

# The command client shares the standard-library transport; no readline or
# color dependencies enter this executable.
fvwmcommand = custom_target(
    'FvwmCommand',
    output: 'FvwmCommand',
    input: fvwmprompt_files,
    command: [
        'env', 'GOCACHE=' + fvwmprompt_go_cache,
        'GOTOOLCHAIN=local', 'GOPROXY=off',
        golang, 'build', '-C', fvwmprompt_build, '-mod=vendor',
        '-ldflags', '-X main.version=' + meson.project_version() + '-' + file_config.get('VERSIONINFO'),
        '-o', meson.current_build_dir() / 'FvwmCommand', './cmd/fvwmcommand',
    ],
    install: true,
    install_dir: bindir,
    install_tag: 'command-client',
)
# Real deadline probes need an uncontended test slot, including under QEMU.
test('command-client', py,
    args: [files('../../tests/test_command_client.py'), '--command', fvwmcommand],
    timeout: 90,
    is_parallel: false,
)

# This standalone menu producer uses only the Go standard library.
fvwm_menu_xlock = custom_target(
    'fvwm-menu-xlock',
    output: 'fvwm-menu-xlock',
    input: fvwmprompt_files,
    command: [
        'env', 'GOCACHE=' + fvwmprompt_go_cache,
        'GOTOOLCHAIN=local', 'GOPROXY=off', 'GOSUMDB=off',
        golang, 'build', '-C', fvwmprompt_build, '-mod=vendor',
        '-ldflags', '-X main.version=' + meson.project_version(),
        '-o', meson.current_build_dir() / 'fvwm-menu-xlock',
        './cmd/fvwmmenuxlock',
    ],
    install: true,
    install_dir: bindir,
    install_tag: 'xlock-menu',
)
test('xlock-menu-contracts', py,
    args: [files('../../tests/test_utility_contracts.py'),
        '--xlock', fvwm_menu_xlock,
        '--xlock-version', meson.project_version()],
    timeout: 60,
)
test('xlock-menu', py,
    args: [files('../../tests/test_xlock_menu.py'), '--xlock', fvwm_menu_xlock],
    timeout: 180,
    is_parallel: false,
)

# Directory menus share the standard-library utility runtime.
fvwm_menu_directory = custom_target(
    'fvwm-menu-directory',
    output: 'fvwm-menu-directory',
    input: fvwmprompt_files,
    command: [
        'env', 'GOCACHE=' + fvwmprompt_go_cache,
        'GOTOOLCHAIN=local', 'GOPROXY=off', 'GOSUMDB=off',
        golang, 'build', '-C', fvwmprompt_build, '-mod=vendor',
        '-ldflags', '-X main.version=' + meson.project_version(),
        '-o', meson.current_build_dir() / 'fvwm-menu-directory',
        './cmd/fvwmmenudirectory',
    ],
    install: true,
    install_dir: bindir,
    install_tag: 'directory-menu',
)
test('directory-menu-contracts', py,
    args: [files('../../tests/test_utility_contracts.py'),
        '--directory', fvwm_menu_directory,
        '--directory-version', meson.project_version()],
    timeout: 60,
)
test('directory-menu', py,
    args: [files('../../tests/test_directory_menu.py'),
        '--directory', fvwm_menu_directory],
    timeout: 180,
)

# Paired with the installed native Script example; no extra Go dependencies.
fvwm_script_com = custom_target(
    'fvwm-script-ComExample',
    output: 'fvwm-script-ComExample',
    input: fvwmprompt_files,
    command: [
        'env', 'GOCACHE=' + fvwmprompt_go_cache,
        'GOTOOLCHAIN=local', 'GOPROXY=off', 'GOSUMDB=off',
        golang, 'build', '-C', fvwmprompt_build, '-mod=vendor',
        '-o', meson.current_build_dir() / 'fvwm-script-ComExample',
        './cmd/fvwmscriptcom',
    ],
    install: true,
    install_dir: fvwm_datadir,
    install_tag: 'script-com-example',
)
test('script-com-example', py,
    args: [files('../../tests/test_script_com_example.py'),
        '--helper', fvwm_script_com],
    timeout: 180,
)

# Coordinated XDG provider/settings port; acquisition has a bounded worker and
# applications stay literal typed records until launch. No new Go dependency.
fvwm_menu_desktop = custom_target(
    'fvwm-menu-desktop',
    output: 'fvwm-menu-desktop',
    input: fvwmprompt_files,
    command: [
        'env', 'GOCACHE=' + fvwmprompt_go_cache,
        'GOTOOLCHAIN=local', 'GOPROXY=off', 'GOSUMDB=off',
        golang, 'build', '-C', fvwmprompt_build, '-mod=vendor',
        '-ldflags', '-X main.version=' + meson.project_version()
            + ' -X main.pngEnabled=' + (libpng.found() ? 'true' : 'false')
            + ' -X main.svgEnabled=' + (librsvg.found() ? 'true' : 'false')
            + ' -X main.xpmEnabled=' + (xpm.found() ? 'true' : 'false'),
        '-o', meson.current_build_dir() / 'fvwm-menu-desktop',
        './cmd/fvwmmenudesktop',
    ],
    install: true,
    install_dir: bindir,
    install_tag: 'desktop-menu',
)
test('desktop-menu', py,
    args: [files('../../tests/test_xdg_desktop.py'),
        '--provider', fvwm_menu_desktop],
    timeout: 180,
)

# Optional native-Form collection consumer and isolated directory/action worker.
# Standard library only; adoption remains an explicit configuration choice.
fvwm_file_browser = custom_target(
    'fvwm-file-browser',
    output: 'fvwm-file-browser',
    input: fvwmprompt_files,
    command: [
        'env', 'GOCACHE=' + fvwmprompt_go_cache,
        'GOTOOLCHAIN=local', 'GOPROXY=off', 'GOSUMDB=off',
        golang, 'build', '-C', fvwmprompt_build, '-mod=vendor',
        '-o', meson.current_build_dir() / 'fvwm-file-browser',
        './cmd/fvwmfilebrowser',
    ],
    install: true,
    install_dir: bindir,
    install_tag: 'file-browser',
)
# Portable folder role is available only with its installed Go consumer.
# Installing an entry never changes user MIME defaults.
install_data('fvwm-file-browser.desktop',
    install_dir: get_option('datadir') / 'applications',
    install_tag: 'file-browser',
)
test('file-browser-policy', golang,
    args: ['test', '-C', fvwmprompt_build, '-json', '-count=1', '-mod=vendor',
        './internal/collection', './internal/filemodel',
        './cmd/fvwmfilebrowser', './cmd/fvwmcommand', './transport'],
    env: prompt_test_env,
    timeout: 180,
)
