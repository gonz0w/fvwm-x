test('command-catalog-contract', py, args: [files('test_command_catalog.py')],
    timeout: 30)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('script-com-example-xvfb', py,
        args: [files('live_x/script_com_example_live.py'),
            '--fvwm', fvwm3, '--mfl', fvwm_mfl, '--script', fvwm_script,
            '--example', meson.project_build_root() / 'modules/FvwmScript/FvwmScript-ComExample',
            '--helper', fvwm_script_com,
            '--artifacts-dir', meson.project_build_root() / 'live-x-script-com-example'],
        timeout: 90,
        is_parallel: false,
    )
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('directory-menu-xvfb', py,
        args: [files('live_x/directory_menu_live.py'),
            '--fvwm', fvwm3, '--mfl', fvwm_mfl,
            '--directory', fvwm_menu_directory,
            '--artifacts-dir', meson.project_build_root() / 'live-x-directory-menu'],
        timeout: 90,
    )
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('xlock-menu-xvfb', py,
        args: [files('live_x/xlock_menu_live.py'),
            '--fvwm', fvwm3, '--mfl', fvwm_mfl,
            '--xlock', fvwm_menu_xlock,
            '--artifacts-dir', meson.project_build_root() / 'live-x-xlock-menu'],
        timeout: 90,
        is_parallel: false,
    )
endif

test('bsd-default-x-contract', py, args: [files('test_bsd_default_x.py')],
    timeout: 180)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('command-client-xvfb', py,
        args: [files('live_x/command_client_live.py'),
            '--fvwm', fvwm3, '--mfl', fvwm_mfl,
            '--command', fvwmcommand,
            '--artifacts-dir', meson.project_build_root() / 'live-x-command-client'],
        timeout: 90,
        is_parallel: false,
    )
endif

test('native-gate-contract', py, args: [files('test_native_gate.py')],
    timeout: 180)

test('resource-profile-report', py, args: [files('test_profile_report.py')],
    timeout: 60)

if get_option('live-x-tests') and libpng.found() and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('config-origin-xvfb', py,
        args: [files('live_x/config_origin_live.py'), '--build', meson.project_build_root(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'config-origin'],
        depends: [fvwm3, fvwm_mfl, fvwm_desktop, fvwm_desktop_worker,
            fvwm_form, fvwm_desktop_capture, fvwm_backer, fvwm_buttons,
            fvwm_event, fvwm_pager, fvwm_iconman],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 240)
    test('browser-continuity-xvfb', py,
        args: [files('live_x/browser_continuity_live.py'),
            '--build', meson.project_build_root(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'browser-continuity'],
        depends: [fvwm3, fvwm_mfl, fvwm_file_browser, fvwm_form,
            fvwm_desktop, fvwm_desktop_worker, fvwm_desktop_capture,
            fvwm_backer, fvwm_buttons, fvwm_event],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 480)
    test('native-layout-xvfb', py,
        args: [files('live_x/native_layout_live.py'), '--build', meson.project_build_root(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'native-layout'],
        depends: [fvwm3, fvwm_mfl, fvwm_file_browser, fvwm_form,
            fvwm_desktop, fvwm_desktop_worker, fvwm_desktop_capture,
            fvwm_backer, fvwm_buttons, fvwm_event, fvwm_pager, fvwm_iconman],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 240)
    test('desktop-inspector-xvfb', py,
        args: [files('live_x/inspector_live.py'), '--build', meson.project_build_root(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'inspector'],
        depends: [fvwm3, fvwm_mfl, fvwm_desktop, fvwm_desktop_worker,
            fvwm_form, fvwm_desktop_capture, fvwm_backer, fvwm_buttons,
            fvwm_event, fvwm_pager, fvwm_iconman],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 300)
    test('theme-composition-xvfb', py,
        args: [files('live_x/theme_composition_live.py'),
            '--build', meson.project_build_root(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'theme-composition'],
        depends: [fvwm3, fvwm_mfl, fvwm_desktop, fvwm_desktop_worker,
            fvwm_form, fvwm_desktop_capture,
            fvwm_backer, fvwm_buttons, fvwm_event, fvwm_pager, fvwm_iconman],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 180)
    test('theme-contrast-xvfb', py,
        args: [files('live_x/theme_composition_live.py'),
            '--contrast', '--build', meson.project_build_root(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'theme-contrast'],
        depends: [fvwm3, fvwm_mfl, fvwm_desktop, fvwm_desktop_worker,
            fvwm_form, fvwm_desktop_capture,
            fvwm_backer, fvwm_buttons, fvwm_event, fvwm_pager, fvwm_iconman],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 300)
endif

if get_option('live-x-tests') and libpng.found() and librsvg.found() and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('named-menu-icons-xvfb', py,
        args: [files('live_x/named_menu_icons_live.py'),
            '--fvwm', fvwm3, '--mfl', fvwm_mfl,
            '--desktop', fvwm_menu_desktop, '--directory', fvwm_menu_directory,
            '--worker', fvwm_svg_worker, '--capture', fvwm_desktop_capture,
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'named-menu-icons'],
        depends: [fvwm3, fvwm_mfl, fvwm_menu_desktop, fvwm_menu_directory,
            fvwm_svg_worker, fvwm_desktop_capture],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 120)
endif

test('xlib-requirement-contract', py,
    args: [files('../tools/maintenance/tests/test_xlib_requirement.py')],
    timeout: 180,
)

test('go-version-contract', py,
    args: [files('../tools/maintenance/tests/test_go_version.py')],
    timeout: 180,
)
test('go-sdk-recipe-contract', py,
    args: [files('test_go_sdk_recipe.py')],
    timeout: 60,
)
if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('ewmh-inventory-xvfb', py,
        args: [files('live_x/ewmh_inventory_live.py'),
               '--fvwm', fvwm3, '--mfl', fvwm_mfl,
               '--ewmh-source', meson.project_source_root() / 'fvwm/ewmh.c',
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts/ewmh-inventory'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
endif

protocol_metadata_probe = executable(
    'protocol-metadata-probe', 'protocol_metadata_probe.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
)
test('protocol-metadata', py,
    args: [files('test_protocol_metadata.py'), '--probe',
           protocol_metadata_probe, '--perl', perl.full_path(),
           '--module-library', meson.project_build_root() / 'perllib/Module.pm'],
)
test('utility-contracts', py, args: [files('test_utility_contracts.py')])

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('prompt-protocol-xvfb', py,
        args: [files('live_x/prompt_protocol_live.py'),
               '--fvwm', fvwm3, '--mfl', fvwm_mfl,
               '--prompt', fvwmprompt,
               '--perl-module', meson.project_build_root() / 'modules/FvwmPerl/FvwmPerl',
               '--perllib', meson.project_source_root() / 'perllib',
               '--module-library', meson.project_build_root() / 'perllib/Module.pm',
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts/prompt-protocol'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
endif

string_copy_test = executable(
    'test-string-copy', ['test_string_copy.c', '../libs/Strings.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG', '-fno-builtin-strlen'],
    link_args: ['-Wl,--wrap=strlen'],
)
test('string-copy', string_copy_test)

wild_test = executable(
    'test-wild', ['test_wild.c', '../libs/wild.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('wild', wild_test)

style_match_test = executable(
    'test-style-match',
    ['test_style_match.c', '../fvwm/style_match.c',
        '../fvwm/style_id_filter.c', '../libs/wild.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG', '-fno-builtin-strlen', '-fno-builtin-strcmp',
        '-fno-builtin-strncmp', '-fno-builtin-strchr'],
    link_args: ['-Wl,--wrap=matchWildcards', '-Wl,--wrap=strlen',
        '-Wl,--wrap=strcmp', '-Wl,--wrap=strncmp', '-Wl,--wrap=strchr'],
)
test('style-match', style_match_test)

style_id_filter_test = executable(
    'test-style-id-filter',
    ['test_style_id_filter.c', '../fvwm/style_id_filter.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
)
test('style-id-filter', style_id_filter_test)

style_flags_test = executable(
    'test-style-flags', 'test_style_flags.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
)
test('style-flags', style_flags_test, timeout: 120)

style_flags_mutate_test = executable(
    'test-style-flags-mutate', 'test_style_flags_mutate.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
)
test('style-flags-mutate', style_flags_mutate_test, timeout: 120)

stack_restack_test = executable(
    'test-stack-restack', ['test_stack_restack.c', '../fvwm/stack_restack.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_args: ['-Wl,--wrap=malloc'],
    c_args: ['-UNDEBUG'],
)
test('stack-restack', stack_restack_test)

stack_window_buffer_test = executable(
    'test-stack-window-buffer',
    ['test_stack_window_buffer.c', '../fvwm/stack_window_buffer.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
    link_args: ['-Wl,--wrap=malloc', '-Wl,--wrap=free'],
)
test('stack-window-buffer', stack_window_buffer_test)

stack_window_list_test = executable(
    'test-stack-window-list', 'test_stack_window_list.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
)
test('stack-window-list', stack_window_list_test)

stack_parent_test = executable(
    'test-stack-parent', ['test_stack_parent.c', '../fvwm/stack_parent.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
    link_args: ['-Wl,--wrap=calloc', '-Wl,--wrap=free'],
)
test('stack-parent', stack_parent_test)

stack_subtree_test = executable(
    'test-stack-subtree', ['test_stack_subtree.c', '../fvwm/stack_subtree.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
    link_args: ['-Wl,--wrap=calloc'],
)
test('stack-subtree', stack_subtree_test)

stack_sort_test = executable(
    'test-stack-sort', ['test_stack_sort.c', '../fvwm/stack_sort.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
)
test('stack-sort', stack_sort_test)

stack_rank_policy_test = executable(
    'test-stack-rank-policy', 'test_stack_rank_policy.c',
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('stack-rank-policy', stack_rank_policy_test)

parent_chain_policy_test = executable(
    'test-parent-chain-policy', 'test_parent_chain_policy.c',
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('parent-chain-policy', parent_chain_policy_test)

atom_batch_test = executable(
    'test-atom-batch', 'test_atom_batch.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=XInternAtom', '-Wl,--wrap=XInternAtoms'],
    c_args: ['-UNDEBUG'],
)
test('atom-batch', atom_batch_test)

json_serialization_test = executable(
    'test-json-serialization', 'test_json_serialization.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('json-serialization', json_serialization_test)

window_children_test = executable(
    'test-window-children', 'test_window_children.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=XGetWindowAttributes',
                '-Wl,--wrap=XQueryTree', '-Wl,--wrap=XFree'],
    c_args: ['-UNDEBUG'],
)
test('window-children', window_children_test)

window_tree_test = executable(
    'test-window-tree', 'test_window_tree.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=XGetWindowAttributes',
        '-Wl,--wrap=XQueryTree', '-Wl,--wrap=XFree'],
    c_args: ['-UNDEBUG'],
)
test('window-tree', window_tree_test)

picture_visual_test = executable(
    'test-picture-visual', 'test_picture_visual.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=XGetVisualInfo', '-Wl,--wrap=XFree',
        '-Wl,--wrap=XAllocColor', '-Wl,--wrap=XFreeColors',
        '-Wl,--wrap=PictureInitColors'],
    c_args: ['-UNDEBUG'],
)
test('picture-visual', picture_visual_test)


property_policy_test = executable(
    'test-property-policy', 'test_property_policy.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('property-policy', property_policy_test)

test('desktop-policy', golang,
    args: ['test', '-C', fvwmprompt_build, '-count=1', '-mod=vendor',
        './internal/desktopcontroller', './internal/viewbinding',
        './internal/actionpalette', './cmd/fvwmdesktop'],
    env: prompt_test_env, timeout: 90)
test('desktop-assets', py, args: [files('test_desktop.py')])
test('responsiveness-profile-evidence', py,
    args: [files('test_responsiveness_profile.py')])

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test(
        'picture-visual-xvfb', py,
        args: [files('live_x/picture_visual_live.py'),
            '--probe', picture_visual_test.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'picture-visual'],
        depends: [picture_visual_test],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 45,
    )
    test(
        'display-ownership-xvfb', py,
        args: [files('live_x/display_ownership_live.py'),
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'display-ownership'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 45,
    )
    test(
        'json-utf8-xvfb', py,
        args: [files('live_x/json_utf8_live.py'),
               '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'json-utf8'],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 35,
    )
    test(
        'scheduler-dispatch-xvfb', py,
        args: [
            files('live_x/scheduler_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'scheduler-dispatch',
        ],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
    selection_probe = executable(
        'test-selection-x', 'test_selection_x.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        c_args: ['-UNDEBUG'],
    )
    test(
        'module-selection-xvfb', py,
        args: [
            files('live_x/selection_live.py'),
            '--probe', selection_probe.full_path(),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--script', fvwm_script.full_path(), '--form', fvwm_form.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'module-selection',
        ],
        depends: [selection_probe, fvwm3, fvwm_mfl, fvwm_script, fvwm_form],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
    x_error_handler_probe = executable(
        'test-x-error-handler', 'test_x_error_handler.c',
        link_args: ['-Wl,--wrap=XCreateGC'],
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        c_args: ['-UNDEBUG'],
    )
    test(
        'x-error-handler-xvfb', py,
        args: [
            files('live_x/x_error_handler_live.py'),
            '--probe', x_error_handler_probe.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'x-error-handler',
        ],
        depends: [x_error_handler_probe],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
    test(
        'desktop-baseline-xvfb', py,
        args: [
            files('live_x/desktop_live.py'), '--build', meson.project_build_root(),
            '--settings-themes',
            '--installed-modules', get_option('prefix') / fvwm_moduledir,
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'desktop',
        ],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        depends: [fvwm3, fvwm_mfl, fvwm_backer, fvwm_buttons, fvwm_event, fvwm_form,
            fvwm_desktop, fvwm_desktop_worker],
        is_parallel: false,
        timeout: host_machine.system() == 'linux' ? 120 : 300,
    )
endif

if host_machine.system() == 'linux'
    readelf = find_program('readelf', 'llvm-readelf')
    test(
        'elf-hardening', py,
        args: [
            files('test_elf_hardening.py'),
            '--targets', meson.project_build_root() / 'meson-info' / 'intro-targets.json',
            '--readelf', readelf.full_path(),
        ],
    )
endif

fd_failure_test = executable(
    'test-fd-failure', ['test_fd_failure.c', '../libs/fd_safety.c'],
    include_directories: includedirs,
    link_args: ['-Wl,--wrap=pipe', '-Wl,--wrap=fcntl',
                '-Wl,--wrap=fstat', '-Wl,--wrap=close',
                '-Wl,--wrap=fcntl64', '-Wl,--wrap=fstat64'],
    c_args: ['-UNDEBUG'],
)
test('fd-failure', fd_failure_test)

module_args_test = executable(
    'test-module-args', 'test_module_args.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=fcntl', '-Wl,--wrap=fcntl64'],
    c_args: ['-UNDEBUG'],
)
test('module-args', module_args_test)

fd_width_test = executable(
    'test-fd-width', 'test_fd_width.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=sysconf', '-Wl,--wrap=getdtablesize'],
    c_args: ['-UNDEBUG'],
)
test('fd-width', fd_width_test)

fd_select_test = executable(
    'test-fd-select', ['test_fd_select.c', '../libs/fd_safety.c'],
    include_directories: includedirs,
    link_args: ['-Wl,--wrap=fcntl', '-Wl,--wrap=fcntl64'],
    c_args: ['-UNDEBUG'],
)
test('fd-select', fd_select_test)

ice_error_test = executable(
    'test-ice-error-policy', 'test_ice_error_policy.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=IceSetIOErrorHandler', '-Wl,--wrap=SmsInitialize'],
    c_args: ['-UNDEBUG'],
)
test('ice-error-policy', ice_error_test)

module_command_write_test = executable(
    'test-module-command-write', 'test_module_command_write.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=write', '-Wl,--wrap=fxmalloc'],
    c_args: ['-UNDEBUG'],
)
test('module-command-write', module_command_write_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    rearrange_signal_probe = executable(
        'rearrange-signal-probe',
        ['rearrange_signal_probe.c', '../modules/FvwmRearrange/FvwmRearrange.c'],
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=signal', '-Wl,--wrap=fopen',
            '-Wl,--wrap=fopen64', '-Wl,--wrap=fclose',
            '-Wl,--wrap=alarm', '-Wl,--wrap=fvwmRead',
            '-Wl,--wrap=fvwmWrite'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'rearrange-signal-xvfb', py,
        args: [
            files('live_x/rearrange_signal_live.py'),
            '--probe', rearrange_signal_probe.full_path(),
            '--artifacts-dir',
            meson.current_build_dir() / 'live-x-artifacts' / 'rearrange-signal',
        ],
        depends: [rearrange_signal_probe],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
    test(
        'rearrange-monitor-xvfb', py,
        args: [
            files('live_x/rearrange_monitor_live.py'),
            '--probe', rearrange_signal_probe.full_path(),
            '--artifacts-dir',
            meson.current_build_dir() / 'live-x-artifacts' / 'rearrange-monitor',
        ],
        depends: [rearrange_signal_probe],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    backer_signal_probe = executable(
        'backer-signal-probe',
        ['backer_signal_probe.c', '../modules/FvwmBacker/FvwmBacker.c',
            '../modules/FvwmBacker/root_bits.c'],
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=signal', '-Wl,--wrap=fvwmRead',
            '-Wl,--wrap=fvwmWrite'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'backer-signal-xvfb', py,
        args: [
            files('live_x/backer_signal_live.py'),
            '--probe', backer_signal_probe.full_path(),
            '--artifacts-dir',
            meson.current_build_dir() / 'live-x-artifacts' / 'backer-signal',
        ],
        depends: [backer_signal_probe],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
endif

module_packet_read_test = executable(
    'test-module-packet-read', 'test_module_packet_read.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=read'],
    c_args: ['-UNDEBUG'],
)
test('module-packet-read', module_packet_read_test, timeout: 15)

module_packet_reader_test = executable(
    'test-module-packet-reader', 'test_module_packet_reader.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=read'],
    c_args: ['-UNDEBUG'],
)
test('module-packet-reader', module_packet_reader_test, timeout: 15)

module_config_line_test = executable(
    'test-module-config-line', 'test_module_config_line.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=read', '-Wl,--wrap=write',
        '-Wl,--wrap=xasprintf'],
    c_args: ['-UNDEBUG'],
)
test('module-config-line', module_config_line_test, timeout: 15)

module_action_expand_test = executable(
    'test-module-action-expand', 'test_module_action_expand.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('module-action-expand', module_action_expand_test)

test('event-transport', py,
    args: [files('test_event_transport.py'), '--module', fvwm_event],
    timeout: 60,
)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    ident_packet_fixture = executable(
        'ident-packet-fixture', 'ident_packet_fixture.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
    )
    test('ident-transport-xvfb', py,
        args: [files('live_x/ident_transport_live.py'), '--module', fvwm_ident,
               '--fixture', ident_packet_fixture,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'ident-transport'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
    test('pager-transport-xvfb', py,
        args: [files('live_x/ident_transport_live.py'), '--pager',
               '--module', meson.project_build_root() / 'modules/FvwmPager/FvwmPager',
               '--fixture', ident_packet_fixture,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'pager-transport'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
    test('ident-partial-xvfb', py,
        args: [files('live_x/ident_transport_live.py'), '--partial-only',
               '--module', fvwm_ident, '--fixture', ident_packet_fixture,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'ident-partial'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    test('iconman-partial-xvfb', py,
        args: [files('live_x/module_partial_live.py'), '--kind', 'iconman',
               '--module', fvwm_iconman, '--fixture', ident_packet_fixture,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'iconman-partial'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )

    test('iconman-service-xvfb', py,
        args: [files('live_x/module_partial_live.py'), '--kind', 'iconman', '--service-only',
               '--module', fvwm_iconman, '--fixture', ident_packet_fixture,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'iconman-service'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )

    test('ident-service-xvfb', py,
        args: [files('live_x/ident_transport_live.py'), '--service-only',
               '--module', fvwm_ident, '--fixture', ident_packet_fixture,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'ident-service'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )

    test('ident-config-xvfb', py,
        args: [files('live_x/ident_config_live.py'), '--module', fvwm_ident,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'ident-config'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
    test('iconman-transport-xvfb', py,
        args: [files('live_x/iconman_transport_live.py'),
               '--module', fvwm_iconman, '--fixture', ident_packet_fixture,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'iconman-transport'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('buttons-keyboard-xvfb', py,
        args: [files('live_x/buttons_keyboard_live.py'),
               '--module', fvwm_buttons, '--fvwm', fvwm3, '--mfl', fvwm_mfl,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'buttons-keyboard'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 90)

    test('buttons-transport-xvfb', py,
        args: [files('live_x/buttons_transport_live.py'),
               '--module', fvwm_buttons,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'buttons-transport'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('script-transport-xvfb', py,
        args: [files('live_x/script_transport_live.py'),
               '--module', fvwm_script,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-transport'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

endif

fd_safety_test = executable(
    'test-fd-safety', ['test_fd_safety.c', '../libs/fd_safety.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('fd-safety', fd_safety_test)

mfl_stream_test = executable(
    'test-mfl-stream',
    ['test_mfl_stream.c', '../modules/FvwmMFL/stream.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
)
test('mfl-stream', mfl_stream_test)

mfl_request_test = executable(
    'test-mfl-request',
    ['test_mfl_request.c', '../modules/FvwmMFL/request.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('mfl-request', mfl_request_test)


configure_merge_test = executable(
    'test-configure-merge', 'test_configure_merge.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
)
test('configure-merge', configure_merge_test)

resize_merge_queue_test = executable(
    'test-resize-merge-queue', 'test_resize_merge_queue.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=XEventsQueued', '-Wl,--wrap=XNextEvent',
                '-Wl,--wrap=XPeekEvent'],
    c_args: ['-UNDEBUG'],
)
test('resize-merge-queue', resize_merge_queue_test)

expose_queue_test = executable(
    'test-expose-queue', 'test_expose_queue.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=XEventsQueued', '-Wl,--wrap=XNextEvent',
                '-Wl,--wrap=XPutBackEvent'],
    c_args: ['-UNDEBUG'],
)
test('expose-queue', expose_queue_test)

fevent_hints_test = executable(
    'test-fevent-hints', 'test_fevent_hints.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('fevent-hints', fevent_hints_test)

image_safety_test = executable(
    'test-image-safety',
    ['test_image_safety.c', '../libs/ImageSafety.c'],
    include_directories: include_directories('..', '../libs'),
    c_args: ['-UNDEBUG'],
)

test('image-safety', image_safety_test)

mfl_event_flags_test = executable(
    'test-mfl-event-flags',
    'test_mfl_event_flags.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
)
test('mfl-event-flags', mfl_event_flags_test)

schedule_policy_test = executable(
    'test-schedule-policy',
    'test_schedule_policy.c',
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('schedule-policy', schedule_policy_test)

monitor_topology_test = executable(
    'test-monitor-topology',
    ['test_monitor_topology.c', '../libs/monitor_topology.c', '../libs/cJSON.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
)

test('monitor-topology', monitor_topology_test)

monitor_scale_test = executable(
    'test-monitor-scale',
    'test_monitor_scale.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('monitor-scale', monitor_scale_test)

wm_ui_metrics_test = executable(
    'test-wm-ui-metrics',
    'test_wm_ui_metrics.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('wm-ui-metrics', wm_ui_metrics_test)

# This profile test assumes all optional desktop rendering backends exist.
# Minimal configurations remain supported and run the other policy tests.
if xcursor.found() and xrender.found() and libpng.found() and librsvg.found()
    supported_features_test = executable(
        'test-supported-features',
        'test_supported_features.c',
        include_directories: includedirs,
        c_args: ['-UNDEBUG'],
    )

    test('supported-features', supported_features_test)
endif

monitor_ownership_test = executable(
    'test-monitor-ownership',
    'test_monitor_ownership.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('monitor-ownership', monitor_ownership_test)

icccm_size_hints_test = executable(
    'test-icccm-size-hints',
    'test_icccm_size_hints.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('icccm-size-hints', icccm_size_hints_test)

geometry_transition_test = executable(
    'test-geometry-transition',
    'test_geometry_transition.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('geometry-transition', geometry_transition_test)

monitor_scale_config_test = executable(
    'test-monitor-scale-config',
    ['test_monitor_scale_config.c', '../fvwm/monitor_scale_config.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG', '-DMONITOR_SCALE_CONFIG_TESTING'],
)

test('monitor-scale-config', monitor_scale_config_test)

preview_policy_test = executable(
    'test-preview-policy',
    ['test_preview_policy.c', '../fvwm/preview_policy.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('preview-policy', preview_policy_test)

palette_policy_test = executable(
    'test-palette-policy',
    ['test_palette_policy.c', '../fvwm/palette_policy.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('palette-policy', palette_policy_test)

action_palette_test = executable(
    'test-action-palette',
    ['test_action_palette.c', '../fvwm/action_palette.c',
     '../fvwm/palette_policy.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('action-palette-snapshots', action_palette_test)

query_json_test = executable(
    'test-query-json', ['test_query_json.c', '../fvwm/query_json.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('core-query-json', query_json_test)

status_json_test = executable(
    'test-status-json',
    ['test_status_json.c', '../fvwm/status_json.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('status-json', status_json_test)

status_pipe_test = executable(
    'test-status-pipe', ['test_status_pipe.c', '../fvwm/status_pipe.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=open', '-Wl,--wrap=open64',
        '-Wl,--wrap=__open_2', '-Wl,--wrap=__open64_2',
        '-Wl,--wrap=write',
        '-Wl,--wrap=fvwm_fd_set_cloexec'],
    c_args: ['-UNDEBUG'],
)
test('status-pipe', status_pipe_test, timeout: 30)

log_stream_test = executable(
    'test-log-stream', 'test_log_stream.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=fdopen', '-Wl,--wrap=fflush'],
    c_args: ['-UNDEBUG'],
)
test('log-stream', log_stream_test)

fscreen_topology_test = executable(
    'test-fscreen-topology',
    'test_fscreen_topology.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('fscreen-topology-adapter', fscreen_topology_test)

module_runtime_test = executable(
    'test-module-runtime',
    ['test_module_runtime.c', '../fvwm/module_runtime.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
    link_args: ['-Wl,--wrap=malloc'],
)

test('module-runtime', module_runtime_test)

module_queue_test = executable(
    'test-module-queue', ['test_module_queue.c', '../fvwm/module_queue.c', '../fvwm/event_loop_policy.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('module-queue-order', module_queue_test)

execcontext_test = executable(
    'test-execcontext', ['test_execcontext.c', '../fvwm/execcontext.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('execcontext-ownership', execcontext_test)

event_loop_policy_test = executable(
    'test-event-loop-policy',
    ['test_event_loop_policy.c', '../fvwm/event_loop_policy.c'],
    include_directories: includedirs,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('event-loop-policy', event_loop_policy_test)

topology_refresh_policy_test = executable(
    'test-topology-refresh-policy',
    ['test_topology_refresh_policy.c', '../fvwm/topology_refresh_policy.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)

test('topology-refresh-policy', topology_refresh_policy_test)

topology_event_adapter_test = executable(
    'test-topology-event-adapter',
    ['test_topology_event_adapter.c', '../fvwm/topology_event_adapter.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('topology-event-adapter', topology_event_adapter_test)

test(
    'linux-router-self-test',
    py,
    args: [files('run_in_linux.py'), '--self-test'],
)
test(
    'live-x-harness-self-test',
    py,
    args: [files('live_x/live_x_harness.py'), '--self-test'],
)
test(
    'pinboard-observer-self-test',
    py,
    args: [files('live_x/pinboard_live.py'), '--self-test'],
)

if get_option('live-x-tests') and host_machine.system() == 'linux'
    dl_dependency = cc.find_library('dl', required: true)
    xrr_query_fault_preload = shared_library(
        'xrr-query-fault-preload',
        'live_x/xrr_query_fault.c',
        include_directories: includedirs,
        dependencies: dl_dependency,
        c_args: ['-DFVWM_XRR_QUERY_FAULT_PRELOAD'],
        name_prefix: '',
        install: false,
    )
    fvwm3_live_x_wrap = executable(
        'fvwm3-live-x-wrap',
        ['live_x/xrr_query_fault.c', 'live_x/module_queue_limits.c'],
        # Faults are injected at link time; only the probe needs the macro.
        objects: fvwm3.extract_all_objects(recursive: false),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        c_args: ['-DFVWM_XRR_QUERY_FAULT_WRAP'],
        link_args: [
            '-Wl,--wrap=XRRGetMonitors',
            '-Wl,--wrap=module_input_queue_ready',
            '-Wl,--wrap=module_command_queue_drain',
        ],
        install: false,
    )
    module_queue_producer = executable(
        'module-queue-producer', 'module_queue_producer.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        install: false,
    )
    test(
        'module-backpressure-xvfb', py,
        args: [
            files('live_x/module_queue_live.py'),
            '--fvwm', fvwm3_live_x_wrap.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--producer', module_queue_producer.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'module-backpressure',
        ],
        depends: [fvwm3_live_x_wrap, fvwm_mfl, module_queue_producer],
        suite: 'live-x',
        is_parallel: false,
        timeout: 60,
    )

    live_x_artifacts = meson.current_build_dir() / 'live-x-artifacts'
    test(
        'scheduler-responsiveness-xvfb',
        py,
        args: [
            files('live_x/responsiveness_profile.py'), '--samples', '8',
            '--fvwm', fvwm3.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir', live_x_artifacts / 'scheduler-responsiveness',
        ],
        depends: [fvwm3, fvwm_mfl],
        suite: 'live-x',
        is_parallel: false,
        timeout: 60,
    )
    test(
        'interactive-geometry-xorg-dummy',
        py,
        args: [
            files('live_x/geometry_transition_live.py'), '--interactive',
            '--fvwm', fvwm3.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--xorg-config', files('live_x/xorg-dummy.conf'),
            '--artifacts-dir', live_x_artifacts / 'interactive-geometry',
        ],
        depends: [fvwm3, fvwm_mfl],
        suite: 'live-x',
        is_parallel: false,
        timeout: 120,
    )
    test(
        'root-fallback-scaled-xvfb',
        py,
        args: [
            files('live_x/geometry_transition_live.py'),
            '--smoke', '--empty-monitors', '--smoke-quarters', '6',
            '--fvwm', fvwm3_live_x_wrap.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir', live_x_artifacts / 'root-fallback-scaled',
        ],
        depends: [fvwm3_live_x_wrap, fvwm_mfl],
        suite: 'live-x',
        is_parallel: false,
        timeout: 90,
    )
    test(
        'root-fallback-xvfb',
        py,
        args: [
            files('live_x/geometry_transition_live.py'),
            '--smoke', '--empty-monitors',
            '--fvwm', fvwm3_live_x_wrap.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir', live_x_artifacts / 'root-fallback',
        ],
        depends: [fvwm3_live_x_wrap, fvwm_mfl],
        suite: 'live-x',
        is_parallel: false,
        timeout: 90,
    )
    test(
        'geometry-transition-xorg-dummy',
        py,
        args: [
            files('live_x/geometry_transition_live.py'),
            '--fvwm', fvwm3.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--xorg-config', files('live_x/xorg-dummy.conf'),
            '--artifacts-dir', live_x_artifacts / 'geometry-transition',
        ],
        depends: [fvwm3, fvwm_mfl],
        suite: 'live-x',
        is_parallel: false,
        timeout: 180,
    )
    test(
        'scale-policy-xorg-dummy',
        py,
        args: [
            files('live_x/live_x_harness.py'),
            '--backend=xorg-dummy',
            '--scenario=scale-policy',
            '--fvwm',
            fvwm3.full_path(),
            '--xorg-config',
            files('live_x/xorg-dummy.conf'),
            '--artifacts-dir',
            live_x_artifacts / 'scale-policy-xorg-dummy',
        ],
        depends: fvwm3,
        suite: 'live-x',
        is_parallel: false,
        timeout: 90,
    )
    test(
        'topology-xvfb',
        py,
        args: [
            files('live_x/live_x_harness.py'),
            '--backend=xvfb',
            '--scenario=topology',
            '--fvwm',
            fvwm3.full_path(),
            '--artifacts-dir',
            live_x_artifacts / 'topology-xvfb',
        ],
        depends: fvwm3,
        suite: 'live-x',
        is_parallel: false,
        timeout: 45,
    )
    test(
        'topology-xorg-dummy',
        py,
        args: [
            files('live_x/live_x_harness.py'),
            '--backend=xorg-dummy',
            '--scenario=topology',
            '--fvwm',
            fvwm3.full_path(),
            '--xorg-config',
            files('live_x/xorg-dummy.conf'),
            '--policy-header',
            files('../fvwm/topology_refresh_policy.h'),
            '--artifacts-dir',
            live_x_artifacts / 'topology-xorg-dummy',
        ],
        depends: fvwm3,
        suite: 'live-x',
        is_parallel: false,
        timeout: 90,
    )
    test(
        'topology-xorg-dummy-failed-query',
        py,
        args: [
            files('live_x/live_x_harness.py'),
            '--backend=xorg-dummy',
            '--scenario=failed-query-recovery',
            '--fvwm',
            fvwm3.full_path(),
            '--wrapped-fvwm',
            fvwm3_live_x_wrap.full_path(),
            '--fault-library',
            xrr_query_fault_preload.full_path(),
            '--xorg-config',
            files('live_x/xorg-dummy.conf'),
            '--artifacts-dir',
            live_x_artifacts / 'topology-xorg-dummy-failed-query',
        ],
        depends: [fvwm3, fvwm3_live_x_wrap, xrr_query_fault_preload],
        suite: 'live-x',
        is_parallel: false,
        timeout: 90,
    )
    test(
        'topology-fault-injection-probe',
        py,
        args: [
            files('live_x/live_x_harness.py'),
            '--backend=xvfb',
            '--scenario=fault-injection-probe',
            '--fvwm',
            fvwm3.full_path(),
            '--wrapped-fvwm',
            fvwm3_live_x_wrap.full_path(),
            '--fault-library',
            xrr_query_fault_preload.full_path(),
            '--artifacts-dir',
            live_x_artifacts,
        ],
        depends: [fvwm3, fvwm3_live_x_wrap, xrr_query_fault_preload],
        suite: 'live-x',
        is_parallel: false,
        timeout: 60,
    )
endif

if get_option('live-x-tests') and host_machine.system() in ['freebsd', 'openbsd']
    test(
        'bsd-x-session',
        py,
        args: [
            files('live_x/geometry_transition_live.py'),
            '--smoke',
            '--fvwm', fvwm3.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir',
            meson.current_build_dir() / 'live-x-artifacts' / 'bsd-session',
        ],
        depends: [fvwm3, fvwm_mfl],
        suite: 'bsd-x',
        is_parallel: false,
        timeout: 90,
    )
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd'] and libpng.found()
    resource_cache_probe = executable(
        'test-resource-cache-x',
        'test_resource_cache_x.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        c_args: ['-UNDEBUG'],
    )
    test(
        'resource-cache-xvfb',
        py,
        args: [
            files('live_x/resource_cache_live.py'),
            '--probe', resource_cache_probe.full_path(),
            '--artifacts-dir',
            meson.current_build_dir() / 'live-x-artifacts' / 'resource-cache',
        ],
        depends: resource_cache_probe,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('mfl-request-xvfb', py,
        args: [files('live_x/mfl_request_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'mfl-request'],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 120,
    )
    test('mfl-preview-xvfb', py,
        args: [files('live_x/mfl_preview_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'mfl-preview'],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    test('mfl-core-state-xvfb', py,
        args: [files('live_x/mfl_core_state_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'mfl-core-state'],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    test('mfl-operations-xvfb', py,
        args: [files('live_x/mfl_operations_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--desktop', fvwm_desktop.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'mfl-operations'],
        depends: [fvwm3, fvwm_mfl, fvwm_desktop_worker],
        env: desktop_worker_env,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    test('mfl-settings-xvfb', py,
        args: [files('live_x/mfl_settings_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--desktop', fvwm_desktop.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'mfl-settings'],
        depends: [fvwm3, fvwm_mfl, fvwm_desktop_worker],
        env: desktop_worker_env,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 120,
    )
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test(
        'mfl-stream-xvfb', py,
        args: [
            files('live_x/mfl_stream_live.py'),
            '--fvwm', fvwm3.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir',
            meson.current_build_dir() / 'live-x-artifacts' / 'mfl-stream',
        ],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 90,
    )
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test(
        'configure-merge-xvfb', py,
        args: [
            files('live_x/configure_merge_live.py'),
            '--fvwm', fvwm3.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir',
            meson.current_build_dir() / 'live-x-artifacts' / 'configure-merge',
        ],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    property_probe = executable(
        'test-property-x', 'test_property_x.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        c_args: ['-UNDEBUG'],
    )
    test(
        'legacy-property-xvfb', py,
        args: [
            files('live_x/legacy_property_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir',
            meson.current_build_dir() / 'live-x-artifacts' / 'legacy-property',
        ],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
    module_fd_probe = executable(
        'module-fd-probe', 'module_fd_probe.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
    )
    test(
        'compatibility-xvfb', py,
        args: [
            files('live_x/compatibility_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--module-probe', module_fd_probe.full_path(),
            '--artifacts-dir',
            meson.current_build_dir() / 'live-x-artifacts' / 'compatibility',
        ],
        depends: [fvwm3, fvwm_mfl, module_fd_probe],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
    test(
        'hardening-xvfb', py,
        args: [
            files('live_x/hardening_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--property-probe', property_probe.full_path(),
            '--module-probe', module_fd_probe.full_path(),
            '--artifacts-dir',
            meson.current_build_dir() / 'live-x-artifacts' / 'hardening',
        ] + (iconv.found() ? [] : ['--without-ewmh-names']),
        depends: [fvwm3, fvwm_mfl, property_probe, module_fd_probe],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd'] and sm.found()
    session_callback_probe = executable(
        'test-session-callbacks', 'test_session_callbacks.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=SmsInitialize',
            '-Wl,--wrap=SmsGetIceConnection',
            '-Wl,--wrap=SmsGenerateClientID',
            '-Wl,--wrap=SmsRegisterClientReply',
            '-Wl,--wrap=SmsSaveYourself',
            '-Wl,--wrap=SmsReturnProperties',
            '-Wl,--wrap=SmsCleanUp',
            '-Wl,--wrap=IceSetShutdownNegotiation',
            '-Wl,--wrap=IceCloseConnection', '-Wl,--wrap=free'],
        c_args: ['-UNDEBUG'],
    )
    foreach scenario : range(5)
        test('session-callbacks-' + scenario.to_string(),
            session_callback_probe, args: [scenario.to_string()], timeout: 30)
    endforeach
    session_startup_probe = executable(
        'test-session-startup', 'test_session_startup.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=SmsInitialize',
            '-Wl,--wrap=IceListenForConnections',
            '-Wl,--wrap=IceGetListenConnectionNumber',
            '-Wl,--wrap=IceFreeListenObjs',
            '-Wl,--wrap=IceGetListenConnectionString',
            '-Wl,--wrap=IceGenerateMagicCookie',
            '-Wl,--wrap=IceSetPaAuthData',
            '-Wl,--wrap=IceSetHostBasedAuthProc',
            '-Wl,--wrap=IceComposeNetworkIdList',
            '-Wl,--wrap=IceAddConnectionWatch',
            '-Wl,--wrap=IceRemoveConnectionWatch',
            '-Wl,--wrap=IceAcceptConnection',
            '-Wl,--wrap=IceConnectionNumber',
            '-Wl,--wrap=IceConnectionStatus',
            '-Wl,--wrap=IceCloseConnection',
            '-Wl,--wrap=fork', '-Wl,--wrap=waitpid', '-Wl,--wrap=setenv'],
        c_args: ['-UNDEBUG'],
    )
    foreach fault : range(9)
        test('session-startup-' + fault.to_string(), session_startup_probe,
            args: [fault.to_string()], timeout: 30)
    endforeach
    session_client_probe = executable(
        'session-client-probe', 'session_client_probe.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        c_args: ['-UNDEBUG'],
    )
    session_probe = executable(
        'session-probe', 'session_probe.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=IceAddConnectionWatch',
            '-Wl,--wrap=IceConnectionNumber'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'session-xvfb', py,
        args: [
            files('live_x/session_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--session-probe', session_probe.full_path(),
            '--client-probe', session_client_probe.full_path(),
            '--module-probe', module_fd_probe.full_path(),
            '--artifacts-dir',
            meson.current_build_dir() / 'live-x-artifacts' / 'session',
        ],
        depends: [fvwm3, fvwm_mfl, session_probe, session_client_probe,
                  module_fd_probe],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
    test('buttons-session-xvfb', py,
        args: [files('live_x/buttons_transport_live.py'), '--session-only',
               '--module', fvwm_buttons, '--session-probe', session_probe,
               '--fvwm', fvwm3,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'buttons-session'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('script-session-xvfb', py,
        args: [files('live_x/script_transport_live.py'), '--session-only',
               '--module', fvwm_script, '--session-probe', session_probe,
               '--fvwm', fvwm3,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-session'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

endif

# Module transport/UI coverage does not require session management.
if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('script-packet-xvfb', py,
        args: [files('live_x/script_transport_live.py'), '--packet-only',
               '--module', fvwm_script,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-packet'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('form-transport-xvfb', py,
        args: [files('live_x/form_transport_live.py'), '--module', fvwm_form,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'form-transport'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('form-input-xvfb', py,
        args: [files('live_x/form_transport_live.py'), '--input-only',
               '--module', fvwm_form,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'form-input'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('form-button-xvfb', py,
        args: [files('live_x/form_transport_live.py'), '--button-only',
               '--module', fvwm_form,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'form-button'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('form-fields-xvfb', py,
        args: [files('live_x/form_transport_live.py'), '--fields-only',
               '--module', fvwm_form,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'form-fields'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('form-data-xvfb', py,
        args: [files('live_x/form_transport_live.py'), '--data-only',
               '--module', fvwm_form,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'form-data'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('form-arrays-xvfb', py,
        args: [files('live_x/form_transport_live.py'), '--arrays-only',
               '--module', fvwm_form,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'form-arrays'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('form-geometry-xvfb', py,
        args: [files('live_x/form_transport_live.py'), '--geometry-only',
               '--module', fvwm_form,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'form-geometry'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('form-command-xvfb', py,
        args: [files('live_x/form_transport_live.py'), '--command-only',
               '--module', fvwm_form,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'form-command'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('form-history-xvfb', py,
        args: [files('live_x/form_transport_live.py'), '--history-only',
               '--module', fvwm_form,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'form-history'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('form-service-xvfb', py,
        args: [files('live_x/form_transport_live.py'), '--service-only',
               '--module', fvwm_form,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'form-service'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('buttons-service-xvfb', py,
        args: [files('live_x/module_partial_live.py'), '--kind', 'buttons',
               '--service-only', '--module', fvwm_buttons,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'buttons-service'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    foreach kind : ['buttons', 'iconman']
        test(kind + '-packet-xvfb', py,
            args: [files('live_x/module_partial_live.py'), '--kind', kind,
                   '--packet-only', '--module', kind == 'buttons' ? fvwm_buttons : fvwm_iconman,
                   '--fixture', ident_packet_fixture,
                   '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / (kind + '-packet')],
            suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
            is_parallel: false, timeout: 60)
    endforeach

    test('pager-layout-xvfb', py,
        args: [files('live_x/pager_layout_live.py'), '--module', fvwm_pager, '--fixture', ident_packet_fixture,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'pager-layout'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('pager-startup-xvfb', py,
        args: [files('live_x/pager_startup_live.py'), '--module', fvwm_pager,
               '--limit', get_option('pager-max-desks').to_string(),
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'pager-startup'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('pager-packet-xvfb', py,
        args: [files('live_x/module_partial_live.py'), '--kind', 'pager',
               '--packet-only', '--module', fvwm_pager, '--fixture', ident_packet_fixture,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'pager-packet'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('pager-service-xvfb', py,
        args: [files('live_x/module_partial_live.py'), '--kind', 'pager',
               '--service-only', '--module', fvwm_pager,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'pager-service'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    foreach name : ['buttons', 'pager']
        test(name + '-partial-xvfb', py,
            args: [files('live_x/module_partial_live.py'), '--kind', name,
                   '--module', meson.project_build_root() / 'modules' / ('Fvwm' + (name == 'buttons' ? 'Buttons' : 'Pager')) / ('Fvwm' + (name == 'buttons' ? 'Buttons' : 'Pager')),
                   '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / (name + '-partial')],
            suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
            is_parallel: false,
            timeout: 60,
        )
    endforeach

    test('script-partial-xvfb', py,
        args: [files('live_x/script_transport_live.py'), '--partial-only',
               '--module', fvwm_script,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-partial'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

    test('form-partial-xvfb', py,
        args: [files('live_x/form_transport_live.py'), '--partial-only',
               '--module', fvwm_form,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'form-partial'],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)

endif

menu_delay_test = executable(
    'test-menu-delay', ['test_menu_delay.c', '../fvwm/menu_delay.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('menu-delay', menu_delay_test)

fqueue_test = executable(
    'test-fqueue', 'test_fqueue.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
    link_args: ['-Wl,--wrap=calloc'],
)
test('fqueue', fqueue_test)

interaction_policy_test = executable(
    'test-interaction-policy',
    ['test_interaction_policy.c', '../fvwm/interaction_policy.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
)
test('interaction-policy', interaction_policy_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    frame_sync_producer = executable(
        'frame-sync-producer', 'frame_sync_producer.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        install: false,
    )
    test(
        'interaction-frame-sync-xvfb', py,
        args: [
            files('live_x/frame_sync_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--producer', frame_sync_producer.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'frame-sync',
        ],
        depends: [fvwm3, fvwm_mfl, frame_sync_producer],
        suite: 'live-x',
        is_parallel: false,
        timeout: 90,
    )
    interaction_producer = executable(
        'interaction-producer', 'interaction_producer.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        install: false,
    )
    test(
        'interaction-servicing-xvfb', py,
        args: [
            files('live_x/interaction_service_live.py'),
            '--fvwm', fvwm3.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--producer', interaction_producer.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'interaction-servicing',
        ],
        depends: [fvwm3, fvwm_mfl, interaction_producer],
        suite: 'live-x',
        is_parallel: false,
        timeout: 90,
    )
    test(
        'button-release-servicing-xvfb', py,
        args: [
            files('live_x/interaction_service_live.py'), '--release-wait',
            '--fvwm', fvwm3.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--producer', interaction_producer.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'button-release-servicing',
        ],
        depends: [fvwm3, fvwm_mfl, interaction_producer],
        suite: 'live-x',
        is_parallel: false,
        timeout: 90,
    )
    test(
        'viewport-servicing-xvfb', py,
        args: [
            files('live_x/interaction_service_live.py'), '--viewport',
            '--fvwm', fvwm3.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--producer', interaction_producer.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'viewport-servicing',
        ],
        depends: [fvwm3, fvwm_mfl, interaction_producer],
        suite: 'live-x',
        is_parallel: false,
        timeout: 90,
    )
    foreach backend : ['xvfb', 'xorg-dummy']
        if backend == 'xvfb' or host_machine.system() == 'linux'
            test(
                'viewport-behavior-' + backend, py,
                args: [files('live_x/viewport_live.py'),
                    '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
                    '--artifacts-dir', meson.current_build_dir() /
                        'live-x-artifacts' / ('viewport-behavior-' + backend)] +
                    (backend == 'xorg-dummy' ?
                        ['--xorg-config', files('live_x/xorg-dummy.conf')] : []),
                depends: [fvwm3, fvwm_mfl],
                suite: 'live-x', is_parallel: false, timeout: 90,
            )
        endif
    endforeach
    test(
        'menu-servicing-xvfb', py,
        args: [files('live_x/interaction_service_live.py'), '--menu',
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--producer', interaction_producer.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'menu-servicing'],
        depends: [fvwm3, fvwm_mfl, interaction_producer],
        suite: 'live-x', is_parallel: false, timeout: 90,
    )
    test(
        'menu-interaction-xvfb', py,
        args: [files('live_x/menu_interaction_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'menu-interaction'],
        depends: [fvwm3, fvwm_mfl],
        suite: 'live-x', is_parallel: false, timeout: 90,
    )

    test('module-logging-xvfb', py,
        args: [files('live_x/module_logging_live.py'), '--fvwm', fvwm3,
            '--mfl', fvwm_mfl, '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'module-logging'],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    test('status-stream-xvfb', py,
        args: [files('live_x/status_stream_live.py'), '--fvwm', fvwm3,
            '--mfl', fvwm_mfl, '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'status-stream'],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 70,
    )
    if host_machine.system() == 'linux'
        test('log-full-xvfb', py,
            args: [files('live_x/status_stream_live.py'), '--full-log',
                '--fvwm', fvwm3, '--mfl', fvwm_mfl, '--artifacts-dir',
                meson.current_build_dir() / 'live-x-artifacts' / 'log-full'],
            depends: [fvwm3, fvwm_mfl], suite: 'live-x',
            is_parallel: false, timeout: 70,
        )
    endif
    module_output_core_sources = []
    foreach source : fvwm_sources
        module_output_core_sources += '../' / source
    endforeach
    module_output_fixture = executable(
        'fvwm-module-output-fixture',
        [module_output_core_sources, 'module_output_start.c'],
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=write'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'expose-redraw-xvfb', py,
        args: [files('live_x/expose_redraw_live.py'),
               '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
               '--artifacts-dir', meson.current_build_dir() /
                   'live-x-artifacts' / 'expose-redraw'],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 50,
    )
    core_readiness_fixture = executable(
        'fvwm-core-readiness-fixture', 'core_readiness_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=poll',
            '-Wl,--wrap=FlushMessageQueue',
            '-Wl,--wrap=module_input_enqueue', '-Wl,--wrap=ProcessICEMsgs'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'core-readiness-xvfb', py,
        args: [files('live_x/core_readiness_live.py'),
            '--fvwm', core_readiness_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'core-readiness'],
        depends: core_readiness_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 45,
    )
    style_index_retention_fixture = executable(
        'fvwm-style-index-retention-fixture', 'style_index_retention_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows',
            '-Wl,--wrap=fxcalloc', '-Wl,--wrap=free',
            '-Wl,--wrap=style_match_prepare',
            '-Wl,--wrap=style_match_lookup_enabled',
            '-Wl,--wrap=style_match_lookup_subject'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'style-index-retention-xvfb', py,
        args: [files('live_x/style_index_retention_live.py'),
            '--fvwm', style_index_retention_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'style-index-retention'],
        depends: style_index_retention_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    style_index_maintenance_fixture = executable(
        'fvwm-style-index-maintenance-fixture', 'style_index_maintenance_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows',
            '-Wl,--wrap=fxcalloc',
            '-Wl,--wrap=style_match_prepare',
            '-Wl,--wrap=style_match_lookup_enabled',
            '-Wl,--wrap=style_match_lookup_subject'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'style-index-maintenance-xvfb', py,
        args: [files('live_x/style_index_maintenance_live.py'),
            '--fvwm', style_index_maintenance_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'style-index-maintenance'],
        depends: style_index_maintenance_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    style_command_owner_fixture = executable(
        'fvwm-style-command-owner-fixture', 'style_command_owner_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows',
            '-Wl,--wrap=GetNextToken', '-Wl,--wrap=free',
            '-Wl,--wrap=MyXGrabServer'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'style-command-owner-xvfb', py,
        args: [files('live_x/style_command_owner_live.py'),
            '--fvwm', style_command_owner_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'style-command-owner'],
        depends: style_command_owner_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    style_update_work_fixture = executable(
        'fvwm-style-update-work-fixture', 'style_update_work_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows',
            '-Wl,--wrap=exc_create_context', '-Wl,--wrap=MyXGrabServer'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'style-update-work-xvfb', py,
        args: [files('live_x/style_update_work_live.py'),
            '--fvwm', style_update_work_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'style-update-work'],
        depends: style_update_work_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    window_update_fixture = executable(
        'fvwm-window-update-fixture', 'window_update_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows',
            '-Wl,--wrap=module_config_dispatch_ready'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'window-update-xvfb', py,
        args: [files('live_x/window_update_live.py'),
            '--fvwm', window_update_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'window-update'],
        depends: window_update_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 45,
    )
    window_tree_fixture = executable(
        'fvwm-window-tree-fixture', 'window_tree_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'window-tree-xvfb', py,
        args: [files('live_x/window_tree_live.py'),
            '--fvwm', window_tree_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'window-tree'],
        depends: window_tree_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 45,
    )
    ewmh_state_fixture = executable(
        'fvwm-ewmh-state-fixture',
        ['ewmh_state_start.c', 'ewmh_publication_probe.c',
            'ewmh_tray_probe.c'],
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows',
            '-Wl,--wrap=ewmh_client_list_append',
            '-Wl,--wrap=ewmh_atom_index_find', '-Wl,--wrap=XChangeProperty',
            '-Wl,--wrap=ewmh_atom_index_sort', '-Wl,--wrap=strcmp',
            '-Wl,--wrap=is_function_allowed', '-Wl,--wrap=strlen',
            '-Wl,--wrap=XSelectInput', '-Wl,--wrap=XFlush',
            '-Wl,--wrap=FiconvCharsetToUtf8', '-Wl,--wrap=free',
            '-Wl,--wrap=fxmalloc', '-Wl,--wrap=EWMH_UpdateWorkArea',
            '-Wl,--wrap=ewmh_ComputeAndSetWorkArea',
            '-Wl,--wrap=ewmh_HandleDynamicWorkArea',
            '-Wl,--wrap=XDeleteProperty',
            '-Wl,--wrap=XInternAtoms', '-Wl,--wrap=XInternAtom',
            '-Wl,--wrap=XGetImage',
            '-Wl,--wrap=malloc', '-Wl,--wrap=realloc'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'ewmh-state-xvfb', py,
        args: [files('live_x/ewmh_state_live.py'),
            '--fvwm', ewmh_state_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'ewmh-state'],
        depends: ewmh_state_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    style_profile_fixture = executable(
        'fvwm-style-profile-fixture',
        ['style_profile_start.c', 'style_merge_probe.c'],
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=strcmp',
            '-Wl,--wrap=strlen', '-Wl,--wrap=strncmp',
            '-Wl,--wrap=style_id_hash', '-Wl,--wrap=style_match_lookup_enabled',
            '-Wl,--wrap=style_match_prepare',
            '-Wl,--wrap=style_match_lookup_subject',
            '-Wl,--wrap=style_id_filter_hash_may_repeat',
            '-Wl,--wrap=style_id_filter_first', '-Wl,--wrap=style_match_id',
            '-Wl,--wrap=style_match_ids_may_overlap',
            '-Wl,--wrap=matchWildcards', '-Wl,--wrap=MyXGrabServer',
            '-Wl,--wrap=MyXUngrabServer'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'style-profile-xvfb', py,
        args: [files('live_x/style_profile_live.py'),
            '--fvwm', style_profile_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'style-profile'],
        depends: style_profile_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 180,
    )
    expose_service_fixture = executable(
        'fvwm-expose-service-fixture', 'expose_service_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows',
                    '-Wl,--wrap=FCollectQueuedExposes',
                    '-Wl,--wrap=DrawIconWindow', '-Wl,--wrap=menu_expose'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'expose-service-xvfb', py,
        args: [files('live_x/expose_service_live.py'),
               '--fvwm', expose_service_fixture.full_path(),
               '--artifacts-dir', meson.current_build_dir() /
                   'live-x-artifacts' / 'expose-service'],
        depends: expose_service_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 40,
    )
    atom_batch_fixture = executable(
        'fvwm-atom-batch-fixture', 'atom_batch_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows',
                    '-Wl,--wrap=XInternAtom', '-Wl,--wrap=XInternAtoms'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'atom-batch-xvfb', py,
        args: [files('live_x/atom_batch_live.py'),
               '--fvwm', atom_batch_fixture.full_path(),
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'atom-batch'],
        depends: atom_batch_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 40,
    )
    exec_startup_probe = executable(
        'test-exec-startup', 'test_exec_startup.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=main', '-Wl,--wrap=fork',
            '-Wl,--wrap=open', '-Wl,--wrap=open64',
            '-Wl,--wrap=__open_2', '-Wl,--wrap=__open64_2',
            '-Wl,--wrap=dup2',
            '-Wl,--wrap=fvwm_setpgrp', '-Wl,--wrap=execl',
            '-Wl,--wrap=fvwm_debug'],
        c_args: ['-UNDEBUG'],
    )
    test('exec-startup', exec_startup_probe, timeout: 30)
    module_sync_fixture = executable(
        'fvwm-module-sync-fixture', 'module_sync_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=write',
            '-Wl,--wrap=read', '-Wl,--wrap=fvwmPoll',
            '-Wl,--wrap=events_monotonic_ns',
            '-Wl,--wrap=module_input_execute'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'module-sync-xvfb', py,
        args: [files('live_x/module_sync_live.py'),
            '--fvwm', module_sync_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'module-sync'],
        depends: module_sync_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    module_startup_producer = executable(
        'module-startup-producer', 'module_startup_producer.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        c_args: ['-UNDEBUG'],
    )
    module_startup_fixture = executable(
        'fvwm-module-startup-fixture', 'module_startup_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=fork',
            '-Wl,--wrap=module_register', '-Wl,--wrap=write',
            '-Wl,--wrap=read', '-Wl,--wrap=fvwmPoll',
            '-Wl,--wrap=events_monotonic_ns',
            '-Wl,--wrap=module_input_execute'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'module-startup-xvfb', py,
        args: [files('live_x/module_startup_live.py'),
            '--fvwm', module_startup_fixture.full_path(),
            '--producer', module_startup_producer.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'module-startup'],
        depends: [module_startup_fixture, module_startup_producer],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    stack_subtree_fixture = executable(
        'fvwm-stack-subtree-fixture', 'stack_subtree_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=calloc'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'stack-subtree-xvfb', py,
        args: [files('live_x/stack_subtree_live.py'),
            '--fvwm', stack_subtree_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'stack-subtree'],
        depends: stack_subtree_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )

    stack_rank_fixture = executable(
        'fvwm-stack-rank-fixture', 'stack_rank_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'stack-rank-xvfb', py,
        args: [files('live_x/stack_rank_live.py'),
            '--fvwm', stack_rank_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'stack-rank'],
        depends: stack_rank_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    stack_window_list_fixture = executable(
        'fvwm-stack-window-list-fixture', 'stack_window_list_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=fxmalloc',
            '-Wl,--wrap=XRestackWindows', '-Wl,--wrap=EWMH_SetWMState',
            '-Wl,--wrap=border_draw_decorations',
            '-Wl,--wrap=focus_grab_buttons_on_layer'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'stack-window-list-xvfb', py,
        args: [files('live_x/stack_window_list_live.py'),
            '--fvwm', stack_window_list_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'stack-window-list'],
        depends: stack_window_list_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 180,
    )
    stack_layer_storage_fixture = executable(
        'fvwm-stack-layer-storage-fixture', 'stack_layer_storage_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=malloc',
            '-Wl,--wrap=XRestackWindows', '-Wl,--wrap=EWMH_SetWMState',
            '-Wl,--wrap=border_draw_decorations',
            '-Wl,--wrap=focus_grab_buttons_on_layer'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'stack-layer-storage-xvfb', py,
        args: [files('live_x/stack_layer_storage_live.py'),
            '--fvwm', stack_layer_storage_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'stack-layer-storage'],
        depends: stack_layer_storage_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 180,
    )
    stack_override_storage_fixture = executable(
        'fvwm-stack-override-storage-fixture', 'stack_override_storage_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=malloc',
            '-Wl,--wrap=focus_grab_buttons_on_layer'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'stack-override-storage-xvfb', py,
        args: [files('live_x/stack_override_storage_live.py'),
            '--fvwm', stack_override_storage_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'stack-override-storage'],
        depends: stack_override_storage_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 180,
    )
    stack_batch_fixture = executable(
        'fvwm-stack-batch-fixture', 'stack_batch_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=fxrealloc',
            '-Wl,--wrap=XRestackWindows', '-Wl,--wrap=focus_grab_buttons_on_layer'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'stack-batch-xvfb', py,
        args: [files('live_x/stack_batch_live.py'),
            '--fvwm', stack_batch_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'stack-batch'],
        depends: stack_batch_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 180,
    )
    stack_restack_fixture = executable(
        'fvwm-stack-restack-fixture', 'stack_restack_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=PositiveWrite',
            '-Wl,--wrap=module_output_enqueue', '-Wl,--wrap=focus_grab_buttons_on_layer'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'stack-restack-xvfb', py,
        args: [files('live_x/stack_restack_live.py'),
            '--fvwm', stack_restack_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'stack-restack'],
        depends: stack_restack_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 180,
    )
    module_constructor_fixture = executable(
        'fvwm-module-constructor-fixture', 'module_constructor_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=fork',
            '-Wl,--wrap=fxrealloc', '-Wl,--wrap=IceSetIOErrorHandler',
            '-Wl,--wrap=SmcOpenConnection'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'module-constructor-xvfb', py,
        args: [files('live_x/module_constructor_live.py'),
            '--fvwm', module_constructor_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'module-constructor'],
        depends: module_constructor_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 180,
    )
    module_packet_fixture = executable(
        'fvwm-module-packet-fixture', 'module_packet_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=PositiveWrite', '-Wl,--wrap=malloc'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'module-packet-xvfb', py,
        args: [files('live_x/module_packet_live.py'),
            '--fvwm', module_packet_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'module-packet'],
        depends: module_packet_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 180,
    )
    module_iteration_fixture = executable(
        'fvwm-module-iteration-fixture', 'module_iteration_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=PositiveWrite'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'module-iteration-xvfb', py,
        args: [files('live_x/module_iteration_live.py'),
            '--fvwm', module_iteration_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'module-iteration'],
        depends: module_iteration_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 180,
    )
    module_storage_fixture = executable(
        'fvwm-module-storage-fixture', 'module_storage_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=malloc',
            '-Wl,--wrap=calloc'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'module-storage-xvfb', py,
        args: [files('live_x/module_storage_live.py'),
            '--fvwm', module_storage_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'module-storage'],
        depends: module_storage_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 180,
    )
    stack_retirement_fixture = executable(
        'fvwm-stack-retirement-fixture', 'stack_retirement_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=PositiveWrite',
            '-Wl,--wrap=focus_grab_buttons_on_layer'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'stack-retirement-xvfb', py,
        args: [files('live_x/stack_retirement_live.py'),
            '--fvwm', stack_retirement_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'stack-retirement'],
        depends: stack_retirement_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 180,
    )
    stack_parent_fixture = executable(
        'fvwm-stack-parent-fixture', 'stack_parent_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows', '-Wl,--wrap=calloc',
            '-Wl,--wrap=focus_grab_buttons_on_layer'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'stack-parent-xvfb', py,
        args: [files('live_x/stack_parent_live.py'),
            '--fvwm', stack_parent_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'stack-parent'],
        depends: stack_parent_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 180,
    )
    stack_redirection_fixture = executable(
        'fvwm-stack-redirection-fixture', 'stack_redirection_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows',
            '-Wl,--wrap=focus_grab_buttons_on_layer'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'stack-redirection-xvfb', py,
        args: [files('live_x/stack_redirection_live.py'),
            '--fvwm', stack_redirection_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'stack-redirection'],
        depends: stack_redirection_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    stack_query_fixture = executable(
        'fvwm-stack-query-fixture', 'stack_query_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows',
            '-Wl,--wrap=XQueryTree', '-Wl,--wrap=XGetWindowAttributes',
            '-Wl,--wrap=XConfigureWindow', '-Wl,--wrap=is_frame_hide_window',
            '-Wl,--wrap=focus_grab_buttons_on_layer'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'stack-query-xvfb', py,
        args: [files('live_x/stack_query_live.py'),
            '--fvwm', stack_query_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'stack-query'],
        depends: stack_query_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    parent_chain_fixture = executable(
        'fvwm-parent-chain-fixture', 'parent_chain_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows',
            '-Wl,--wrap=XFindContext'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'parent-chain-xvfb', py,
        args: [files('live_x/parent_chain_live.py'),
            '--fvwm', parent_chain_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'parent-chain'],
        depends: parent_chain_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    client_identity_fixture = executable(
        'fvwm-client-identity-fixture', 'client_identity_start.c',
        objects: module_output_fixture.extract_objects(module_output_core_sources),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=CaptureAllWindows',
            '-Wl,--wrap=XFindContext'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'client-identity-xvfb', py,
        args: [files('live_x/client_identity_live.py'),
            '--fvwm', client_identity_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'client-identity'],
        depends: client_identity_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    test(
        'module-output-xvfb', py,
        args: [files('live_x/module_output_live.py'),
            '--fvwm', module_output_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'module-output'],
        depends: module_output_fixture,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    test(
        'raise-unmanaged-xvfb', py,
        args: [files('live_x/raise_unmanaged_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'raise-unmanaged'],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60,
    )
    test(
        'parent-layer-xvfb', py,
        args: [files('live_x/parent_layer_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'parent-layer'],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 90,
    )
    test(
        'layer-boundary-xvfb', py,
        args: [files('live_x/layer_boundary_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'layer-boundary'],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 90,
    )
    test(
        'stack-layer-xvfb', py,
        args: [files('live_x/stack_layer_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'stack-layer'],
        depends: [fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 90,
    )
    interaction_x_probe = executable(
        'test-interaction-x',
        ['test_interaction_x.c', '../fvwm/interaction.c', '../fvwm/interaction_policy.c'],
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        c_args: ['-UNDEBUG'],
    )
    test(
        'interaction-cancellation-xvfb', py,
        args: [
            files('live_x/interaction_cancel_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--interaction-probe', interaction_x_probe.full_path(),
            '--artifacts-dir',
            meson.current_build_dir() / 'live-x-artifacts' / 'interaction-cancellation',
        ],
        depends: [fvwm3, fvwm_mfl, interaction_x_probe],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 120,
    )
endif

script_file_test = executable(
    'test-script-file',
    ['test_script_file.c', '../modules/FvwmScript/script_file.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('script-file', script_file_test)

script_command_test = executable(
    'test-script-command',
    ['test_script_command.c', '../modules/FvwmScript/script_command.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('script-command', script_command_test)

script_output_test = executable(
    'test-script-output',
    ['test_script_output.c', '../modules/FvwmScript/script_output.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('script-output', script_output_test)

script_index_test = executable(
    'test-script-index',
    ['test_script_index.c', '../modules/FvwmScript/script_index.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('script-index', script_index_test)

script_parser_test = executable(
    'test-script-parser',
    ['test_script_parser.c', '../modules/FvwmScript/script.c',
     '../modules/FvwmScript/scanner.c', '../modules/FvwmScript/libyywrap.c',
     '../modules/FvwmScript/script_storage.c',
     '../modules/FvwmScript/script_value.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('script-parser', script_parser_test, timeout: 60)

script_value_owner_test = executable(
    'test-script-value-owner',
    ['test_script_value_owner.c', '../modules/FvwmScript/script_value_table.c',
     '../modules/FvwmScript/script.c', '../modules/FvwmScript/scanner.c',
     '../modules/FvwmScript/libyywrap.c',
     '../modules/FvwmScript/script_storage.c',
     '../modules/FvwmScript/script_value.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG', '-fno-builtin-free'],
    link_args: ['-Wl,--wrap=free', '-Wl,--wrap=yylex'],
)
test('script-value-owner', script_value_owner_test, timeout: 60)

script_property_owner_test = executable(
    'test-script-property-owner',
    ['test_script_property_owner.c', '../modules/FvwmScript/script.c',
     '../modules/FvwmScript/scanner.c', '../modules/FvwmScript/libyywrap.c',
     '../modules/FvwmScript/script_storage.c',
     '../modules/FvwmScript/script_value.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG', '-fno-builtin-free'],
    link_args: ['-Wl,--wrap=free', '-Wl,--wrap=yylex',
        '-Wl,--wrap=FGettext', '-Wl,--wrap=FGettextCopy',
        '-Wl,--wrap=fxstrdup'],
)
test('script-property-owner', script_property_owner_test, timeout: 60)

script_property_module_probe = executable(
    'test-script-property-module-probe', 'test_script_property_owner.c',
    objects: fvwm_script.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: [libfvwm3, libWidgets],
    c_args: ['-UNDEBUG', '-fno-builtin-free',
        '-DSCRIPT_PROPERTY_MODULE_PROBE'],
    link_args: ['-Wl,--wrap=main', '-Wl,--wrap=yyparse',
        '-Wl,--wrap=ChooseFunction', '-Wl,--wrap=free'],
)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('script-property-owner-xvfb', py,
        args: [files('live_x/script_property_owner_live.py'), '--module',
            script_property_module_probe, '--require-ownership',
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-property-owner'],
        depends: script_property_module_probe,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 90)
endif

# Observe synchronous WM metadata ownership on actual production objects.
metadata_owner_wraps = [
    '-Wl,--wrap=main', '-Wl,--wrap=XAllocSizeHints',
    '-Wl,--wrap=XAllocWMHints', '-Wl,--wrap=XStringListToTextProperty',
    '-Wl,--wrap=XSetWMProperties', '-Wl,--wrap=XGetWMSizeHints',
    '-Wl,--wrap=XFree', '-Wl,--wrap=XMapRaised', '-Wl,--wrap=XMapWindow',
]
script_metadata_owner_probe = executable(
    'test-script-metadata-owner', 'test_module_metadata_owner.c',
    objects: fvwm_script.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: [libfvwm3, libWidgets],
    c_args: ['-UNDEBUG', '-DMETADATA_SCRIPT'],
    link_args: metadata_owner_wraps,
)
ident_metadata_owner_probe = executable(
    'test-ident-metadata-owner', 'test_module_metadata_owner.c',
    objects: fvwm_ident.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG', '-DMETADATA_IDENT'],
    link_args: metadata_owner_wraps + ['-Wl,--wrap=FlocaleDrawString'],
)
iconman_metadata_owner_probe = executable(
    'test-iconman-metadata-owner', 'test_module_metadata_owner.c',
    objects: fvwm_iconman.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG', '-DMETADATA_ICONMAN'],
    link_args: metadata_owner_wraps,
)
if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('module-metadata-owner-xvfb', py,
        args: [files('live_x/module_metadata_owner_live.py'),
            '--script', script_metadata_owner_probe,
            '--ident', ident_metadata_owner_probe,
            '--iconman', iconman_metadata_owner_probe,
            '--fixture', ident_packet_fixture,
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'module-metadata-owner'],
        depends: [script_metadata_owner_probe, ident_metadata_owner_probe,
            iconman_metadata_owner_probe, ident_packet_fixture],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 120)
endif

script_block_owner_test = executable(
    'test-script-block-owner',
    ['test_script_block_owner.c', '../modules/FvwmScript/script_block.c',
     '../modules/FvwmScript/script.c', '../modules/FvwmScript/scanner.c',
     '../modules/FvwmScript/libyywrap.c',
     '../modules/FvwmScript/script_storage.c',
     '../modules/FvwmScript/script_value.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG', '-fno-builtin-free'],
    link_args: ['-Wl,--wrap=free'],
)
test('script-block-owner', script_block_owner_test, timeout: 60)

script_process_test = executable(
    'test-script-process',
    ['test_script_process.c', '../modules/FvwmScript/script_process.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('script-process', script_process_test, timeout: 60)

if host_machine.system() == 'linux'
    script_process_fault_test = executable(
        'test-script-process-fault',
        ['test_script_process_fault.c', '../modules/FvwmScript/script_process.c'],
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        c_args: ['-UNDEBUG'],
        link_args: ['-Wl,--wrap=pipe', '-Wl,--wrap=fcntl',
            '-Wl,--wrap=fcntl64', '-Wl,--wrap=sigprocmask', '-Wl,--wrap=sigaction',
            '-Wl,--wrap=fork', '-Wl,--wrap=setpgid', '-Wl,--wrap=dup2',
            '-Wl,--wrap=execl', '-Wl,--wrap=fvwmPoll', '-Wl,--wrap=poll'],
    )
    test('script-process-fault', script_process_fault_test, timeout: 60)
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test(
        'script-process-xvfb',
        py,
        args: [files('live_x/script_process_live.py'), '--module', fvwm_script,
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-process'],
        depends: fvwm_script,
        is_parallel: false,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        timeout: 90,
    )
endif

script_value_test = executable(
    'test-script-value',
    ['test_script_value.c', '../modules/FvwmScript/script_value.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('script-value', script_value_test)

script_message_test = executable(
    'test-script-message',
    ['test_script_message.c', '../modules/FvwmScript/script_message.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('script-message', script_message_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('script-message-xvfb', py,
        args: [files('live_x/script_message_live.py'), '--module', fvwm_script,
               '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-message'],
        depends: fvwm_script,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)
endif

script_message_owner_test = executable(
    'test-script-message-owner', 'test_script_message_owner.c',
    objects: fvwm_script.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: [libfvwm3, libWidgets],
    link_args: ['-Wl,--wrap=main', '-Wl,--wrap=GetNextToken', '-Wl,--wrap=free'],
    c_args: ['-UNDEBUG'],
)
test('script-message-owner', script_message_owner_test)

script_binding_owner_test = executable(
    'test-script-binding-owner', 'test_script_binding_owner.c',
    objects: fvwm_script.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: [libfvwm3, libWidgets],
    link_args: ['-Wl,--wrap=main', '-Wl,--wrap=free',
        '-Wl,--wrap=FvwmStringToKeysym', '-Wl,--wrap=AddBinding'],
    c_args: ['-UNDEBUG', '-fno-builtin-free'],
)
test('script-binding-owner', script_binding_owner_test)

color_status_test = executable(
    'test-color-status', 'test_color_status.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=XParseColor', '-Wl,--wrap=XQueryColor',
        '-Wl,--wrap=PictureAllocColor'],
    c_args: ['-UNDEBUG'],
)
test('color-status', color_status_test)

script_base_owner_test = executable(
    'test-script-base-owner', 'test_script_base_owner.c',
    objects: fvwm_script.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: [libfvwm3, libWidgets],
    link_args: ['-Wl,--wrap=main', '-Wl,--wrap=free',
        '-Wl,--wrap=XCreateFontCursor', '-Wl,--wrap=XFreeCursor',
        '-Wl,--wrap=XCreateWindow', '-Wl,--wrap=fvwmlib_XCreateGC',
        '-Wl,--wrap=XFreeGC', '-Wl,--wrap=fvwmlib_get_color',
        '-Wl,--wrap=GetColor', '-Wl,--wrap=XSelectInput',
        '-Wl,--wrap=XStringListToTextProperty', '-Wl,--wrap=XSetWMProperties',
        '-Wl,--wrap=XInternAtom', '-Wl,--wrap=XSetWMProtocols',
        '-Wl,--wrap=SetWindowBackground', '-Wl,--wrap=PictureFreeColors'],
    c_args: ['-UNDEBUG', '-fno-builtin-free'],
)
test('script-base-owner', script_base_owner_test)

script_base_lifecycle_probe = executable(
    'test-script-base-lifecycle', 'test_script_base_lifecycle.c',
    objects: fvwm_script.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: [libfvwm3, libWidgets],
    link_args: ['-Wl,--wrap=main', '-Wl,--wrap=yyparse', '-Wl,--wrap=free',
        '-Wl,--wrap=CopyStringWithQuotes', '-Wl,--wrap=ChooseFunction',
        '-Wl,--wrap=XCreateFontCursor', '-Wl,--wrap=XFreeCursor',
        '-Wl,--wrap=fvwmlib_XCreateGC', '-Wl,--wrap=XFreeGC',
        '-Wl,--wrap=XInternAtom', '-Wl,--wrap=XSetSelectionOwner',
        '-Wl,--wrap=script_widget_prepare', '-Wl,--wrap=SendText',
        '-Wl,--wrap=SendFinishedStartupNotification', '-Wl,--wrap=XFlush',
        '-Wl,--wrap=XUnmapWindow', '-Wl,--wrap=XCloseDisplay',
        '-Wl,--wrap=XSetErrorHandler', '-Wl,--wrap=PrintXErrorAndCoredump',
        '-Wl,--wrap=PictureFreeColors'],
    c_args: ['-UNDEBUG', '-fno-builtin-free'],
)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('script-base-owner-xvfb', py,
        args: [files('live_x/script_base_owner_live.py'), '--module',
            script_base_lifecycle_probe, '--color-probe', color_status_test,
            '--artifacts-dir',
            meson.current_build_dir() / 'live-x-artifacts' / 'script-base-owner'],
        depends: [script_base_lifecycle_probe, color_status_test],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 120)
endif

script_widget_owner_test = executable(
    'test-script-widget-owner', 'test_script_widget_owner.c',
    objects: fvwm_script.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: [libfvwm3, libWidgets],
    link_args: ['-Wl,--wrap=main', '-Wl,--wrap=FlocaleUnloadFont',
        '-Wl,--wrap=XFreeGC', '-Wl,--wrap=XDestroyWindow',
        '-Wl,--wrap=XKillClient', '-Wl,--wrap=XCreateFontCursor',
        '-Wl,--wrap=XFreeCursor', '-Wl,--wrap=XCreateWindow',
        '-Wl,--wrap=fvwmlib_XCreateGC', '-Wl,--wrap=GetColor',
        '-Wl,--wrap=XSetForeground', '-Wl,--wrap=XSetBackground',
        '-Wl,--wrap=XSetLineAttributes', '-Wl,--wrap=XSelectInput',
        '-Wl,--wrap=free', '-Wl,--wrap=FlocaleLoadFont',
        '-Wl,--wrap=FlocaleTextWidth', '-Wl,--wrap=PictureFreeColors',
        '-Wl,--wrap=XFreePixmap', '-Wl,--wrap=XClearWindow'],
    c_args: ['-UNDEBUG'],
)
test('script-widget-owner', script_widget_owner_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('script-widget-owner-xvfb', py,
        args: [files('live_x/script_widget_owner_live.py'), '--module', fvwm_script,
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-widget-owner'],
        depends: fvwm_script,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 90)
endif

script_icon_owner_test = executable(
    'test-script-icon-owner', 'test_script_icon_owner.c',
    objects: fvwm_script.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: [libfvwm3, libWidgets],
    link_args: ['-Wl,--wrap=main', '-Wl,--wrap=free',
        '-Wl,--wrap=XFreePixmap', '-Wl,--wrap=PictureFreeColors',
        '-Wl,--wrap=PictureFindImageFile',
        '-Wl,--wrap=PImageLoadPixmapFromFile'],
    c_args: ['-UNDEBUG'],
)
test('script-icon-owner', script_icon_owner_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('script-icon-xvfb', py,
        args: [files('live_x/script_icon_live.py'), '--module', fvwm_script,
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-icon'],
        depends: fvwm_script,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 60)
endif


script_execution_test = executable(
    'test-script-execution',
    ['test_script_execution.c', '../modules/FvwmScript/script_execution.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('script-execution', script_execution_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test(
        'script-execution-xvfb', py,
        args: [
            files('live_x/script_execution_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--script', fvwm_script.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-execution',
        ],
        depends: [fvwm3, fvwm_mfl, fvwm_script],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 60,
    )
endif

script_storage_test = executable(
    'test-script-storage',
    ['test_script_storage.c', '../modules/FvwmScript/script_storage.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('script-storage', script_storage_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test(
        'script-storage-xvfb', py,
        args: [
            files('live_x/script_storage_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--script', fvwm_script.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-storage',
        ],
        depends: [fvwm3, fvwm_mfl, fvwm_script],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 90,
    )
endif

script_text_test = executable(
    'test-script-text',
    ['test_script_text.c', '../modules/FvwmScript/script_text.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('script-text', script_text_test)

script_widget_math_test = executable(
    'test-script-widget-math',
    ['test_script_widget_math.c', '../modules/FvwmScript/script_widget_math.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('script-widget-math', script_widget_math_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test(
        'script-text-list-xvfb', py,
        args: [
            files('live_x/script_text_list_live.py'),
            '--fvwm', fvwm3.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--script', fvwm_script.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-text-list',
        ],
        depends: [fvwm3, fvwm_mfl, fvwm_script],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        timeout: 90, is_parallel: false,
    )
    test(
        'script-widgets-xvfb', py,
        args: [
            files('live_x/script_widgets_live.py'),
            '--fvwm', fvwm3.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--script', fvwm_script.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-widgets',
        ],
        depends: [fvwm3, fvwm_mfl, fvwm_script],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        timeout: 90, is_parallel: false,
    )
endif

script_delay_test = executable(
    'test-script-delay',
    ['test_script_delay.c', '../modules/FvwmScript/script_delay.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('script-delay', script_delay_test)

colorset_store_test = executable(
    'test-colorset-store',
    ['test_colorset_store.c', '../libs/colorset_store.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('colorset-store', colorset_store_test)

colorset_api_test = executable(
    'test-colorset-api', 'test_colorset_api.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('colorset-api', colorset_api_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    colorset_probe = executable(
        'colorset-probe', 'colorset_probe.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
    )
    test(
        'colorset-xvfb', py,
        args: [
            files('live_x/colorset_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--script', fvwm_script.full_path(), '--probe', colorset_probe.full_path(),
            '--limit', get_option('colorset-limit').to_string(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'colorset',
        ],
        depends: [fvwm3, fvwm_mfl, fvwm_script, colorset_probe],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        timeout: 180, is_parallel: false,
    )
endif

script_fifo_test = executable(
    'test-script-fifo',
    ['test_script_fifo.c', '../modules/FvwmScript/script_fifo.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('script-fifo', script_fifo_test, timeout: 60)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test(
        'script-fifo-xvfb', py,
        args: [
            files('live_x/script_fifo_live.py'),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--script', fvwm_script.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-fifo',
        ],
        depends: [fvwm3, fvwm_mfl, fvwm_script],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        timeout: 120, is_parallel: false,
    )
endif

if libpng.found()
    png_setup_test = executable(
        'test-png-setup', 'test_png_setup.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: [
            '-Wl,--wrap=png_create_read_struct',
            '-Wl,--wrap=png_create_info_struct',
            '-Wl,--wrap=png_set_error_fn',
            '-Wl,--wrap=png_destroy_read_struct',
        ],
        c_args: ['-UNDEBUG'],
    )
    test('png-setup-recovery', png_setup_test)
endif

test(
    'verification-profile', py,
    args: files('test_verification_profile.py'),
    # Four bounded 15-second wrapper runs plus fixture startup.
    timeout: 75,
)

# Shared acquisition policy used by the image decoder handoffs.
image_input_test = executable(
    'test-image-input',
    ['test_image_input.c', '../libs/image_input.c', '../libs/fd_safety.c'],
    include_directories: includedirs,
    link_args: ['-Wl,--wrap=open', '-Wl,--wrap=open64',
                '-Wl,--wrap=__open_2', '-Wl,--wrap=__open64_2',
                '-Wl,--wrap=fcntl', '-Wl,--wrap=fcntl64',
                '-Wl,--wrap=fstat', '-Wl,--wrap=fstat64',
                '-Wl,--wrap=read', '-Wl,--wrap=__read_chk',
                '-Wl,--wrap=close', '-Wl,--wrap=realloc',
                '-Wl,--wrap=free'],
    c_args: ['-UNDEBUG'],
)
test('image-input', image_input_test, timeout: 15)

image_decompress_test = executable(
    'test-image-decompress',
    ['test_image_decompress.c', '../libs/image_decompress.c', '../libs/image_process.c',
     '../libs/fvwmsignal.c',
     '../libs/image_input.c', '../libs/fd_safety.c'],
    include_directories: includedirs,
    link_args: ['-Wl,--wrap=fork', '-Wl,--wrap=realloc'],
    c_args: ['-UNDEBUG'],
)
test('image-decompress', image_decompress_test, timeout: 20)

image_xbm_test = executable(
    'test-image-xbm',
    ['test_image_xbm.c', '../libs/image_xbm.c', '../libs/ImageSafety.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
)
test('image-xbm', image_xbm_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    image_input_probe = executable(
        'test-image-input-x', 'test_image_input_x.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=read', '-Wl,--wrap=XcursorImagesLoadCursor',
                    '-Wl,--wrap=XcursorImageLoadCursor'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'image-input-xvfb', py,
        args: [
            files('live_x/image_input_live.py'),
            '--probe', image_input_probe.full_path(),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'image-input',
        ],
        depends: [image_input_probe, fvwm3, fvwm_mfl],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: 120,
    )
endif

image_svg_protocol_test = executable(
    'test-image-svg-protocol',
    ['test_image_svg_protocol.c', '../libs/image_svg_protocol.c',
     '../libs/ImageSafety.c'],
    include_directories: includedirs,
    link_args: ['-Wl,--wrap=malloc'],
    c_args: ['-UNDEBUG'],
)
test('image-svg-protocol', image_svg_protocol_test)

# The exec'd test child must fit the real resource limit; ASan's shadow address
# space cannot. Sanitized transport and protocol are covered by the other units.
image_process_test = executable(
    'test-image-process',
    ['test_image_process.c', '../libs/image_decompress.c', '../libs/image_process.c',
     '../libs/fvwmsignal.c',
     '../libs/image_input.c', '../libs/fd_safety.c'],
    include_directories: includedirs,
    link_args: ['-Wl,--wrap=getrlimit', '-Wl,--wrap=getrlimit64'],
    override_options: ['b_sanitize=none'],
    c_args: ['-UNDEBUG'],
)
test('image-process', image_process_test, timeout: 20)

if librsvg.found() and get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test(
        'image-svg-isolation-xvfb', py,
        args: [
            files('live_x/image_svg_live.py'),
            '--probe', image_input_probe.full_path(),
            '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'image-svg',
        ],
        depends: [image_input_probe, fvwm3, fvwm_mfl, fvwm_svg_worker],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false,
        timeout: get_option('image-svg-timeout') * 2 + 60,
    )
endif

if librsvg.found()
    # Exercise the decoder directly under normal sanitizer builds; only the
    # resource-limited production worker excludes ASan's huge shadow mapping.
    image_svg_render_test = executable(
        'test-image-svg-render',
        ['test_image_svg_render.c', '../libs/image_svg.c',
         '../libs/ImageSafety.c', '../libs/monitor_scale.c'],
        include_directories: includedirs,
        dependencies: image_renderer_deps,
        c_args: ['-UNDEBUG'],
    )
    test('image-svg-render', image_svg_render_test)
endif

image_svg_client_test = executable(
    'test-image-svg-client',
    ['test_image_svg_client.c', '../libs/image_svg_client.c',
     '../libs/image_svg_protocol.c', '../libs/ImageSafety.c'],
    include_directories: includedirs,
    dependencies: summary_depvals['x11'],
    c_args: ['-UNDEBUG'],
)
test('image-svg-client', image_svg_client_test)

script_geometry_test = executable(
    'test-script-geometry',
    ['test_script_geometry.c', '../modules/FvwmScript/script_geometry.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('script-geometry', script_geometry_test)

font_geometry_x_test = executable(
    'test-font-geometry-x',
    'test_font_geometry_x.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=XftTextExtents32', '-Wl,--wrap=XwcTextEscapement'],
    c_args: ['-UNDEBUG'],
)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test(
        'script-geometry-xvfb', py,
        args: [
            files('live_x/script_geometry_live.py'),
            '--fvwm', fvwm3.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--script', fvwm_script.full_path(),
            '--font-probe', font_geometry_x_test.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-geometry',
        ],
        depends: [fvwm3, fvwm_mfl, fvwm_script, font_geometry_x_test],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        timeout: 180, is_parallel: false,
    )
endif

font_shadow_test = executable(
    'test-font-shadow',
    ['test_font_shadow.c', '../libs/font_shadow.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('font-shadow', font_shadow_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test(
        'font-shadow-consumers-xvfb', py,
        args: [
            files('live_x/font_shadow_live.py'),
            '--fvwm', fvwm3.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--modules', meson.project_build_root() / 'modules',
            '--shadow-limit', get_option('font-shadow-limit').to_string(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'font-shadow',
        ],
        depends: [fvwm3, fvwm_mfl, fvwm_script, fvwm_pager,
            fvwm_form, fvwm_buttons, fvwm_iconman, fvwm_ident],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        timeout: 240, is_parallel: false,
    )
endif

image_process_async_test = executable(
    'test-image-process-async',
    ['test_image_process_async.c', '../libs/image_process.c',
     '../libs/fvwmsignal.c', '../libs/image_input.c', '../libs/fd_safety.c'],
    include_directories: includedirs,
    link_args: ['-Wl,--wrap=fork', '-Wl,--wrap=kill'],
    c_args: ['-UNDEBUG'],
)
test('image-process-async', image_process_async_test, timeout: 25)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    picture_argb_probe = executable(
        'test-picture-argb-x', 'test_picture_argb_x.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=XPutImage', '-Wl,--wrap=XFreeColors'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'picture-argb-xvfb', py,
        args: [files('live_x/picture_argb_live.py'),
            '--probe', picture_argb_probe.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'picture-argb'],
        depends: picture_argb_probe,
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        timeout: 150, is_parallel: false,
    )
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd'] and librsvg.found()
    image_async_worker_fixture = executable(
        'image-async-worker-fixture',
        ['image_async_worker_fixture.c', '../libs/image_svg_protocol.c',
         '../libs/ImageSafety.c'],
        include_directories: includedirs,
        c_args: ['-UNDEBUG'],
        override_options: ['b_sanitize=none'],
    )
    picture_async_probe = executable(
        'test-picture-async-x', 'test_picture_async_x.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=fork', '-Wl,--wrap=XCreateWindow'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'picture-async-xvfb', py,
        args: [files('live_x/picture_async_live.py'),
            '--probe', picture_async_probe.full_path(),
            '--worker', image_async_worker_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'picture-async'],
        depends: [picture_async_probe, image_async_worker_fixture],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        timeout: 120, is_parallel: false,
    )
endif

fvwmsignal_test = executable(
    'test-fvwmsignal',
    ['test_fvwmsignal.c', '../libs/fvwmsignal.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('fvwmsignal', fvwmsignal_test, timeout: 15)

image_async_wait_test = executable(
    'test-image-async-wait',
    ['test_image_async_wait.c', '../fvwm/image_async_wait.c',
     '../libs/fvwmsignal.c'],
    include_directories: includedirs,
    link_args: ['-Wl,--wrap=poll', '-Wl,--wrap=select'],
    c_args: ['-UNDEBUG'],
)
test('image-async-wait', image_async_wait_test, timeout: 15)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd'] and librsvg.found()
    image_async_core_probe = executable(
        'test-image-async-core-x',
        ['test_image_async_core_x.c', '../fvwm/image_async.c',
         '../fvwm/image_async_wait.c'],
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=fork'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'image-async-core-xvfb', py,
        args: [files('live_x/picture_async_live.py'),
            '--probe', image_async_core_probe.full_path(),
            '--worker', image_async_worker_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'image-async-core'],
        depends: [image_async_core_probe, image_async_worker_fixture],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        timeout: 90, is_parallel: false,
    )
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    if libpng.found()
        image_owner_probe = executable(
            'image-owner-probe', 'image_owner_probe.c',
            include_directories: includedirs,
            dependencies: all_found_deps,
            link_with: libfvwm3,
        )
        test(
            'image-owners-xvfb', py,
            args: [files('live_x/image_owners_live.py'),
                '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
                '--probe', image_owner_probe.full_path(),
                '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'image-owners'],
            depends: [fvwm3, fvwm_mfl, image_owner_probe],
            suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
            is_parallel: false, timeout: 45,
        )
    endif
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd'] and librsvg.found()
    if libpng.found()
        image_colorset_fixture = executable(
            'fvwm-image-colorset-fixture', 'image_colorset_start.c',
            objects: fvwm3.extract_all_objects(recursive: false),
            include_directories: includedirs,
            dependencies: all_found_deps,
            link_with: libfvwm3,
            link_args: ['-Wl,--wrap=CaptureAllWindows',
                '-Wl,--wrap=FRenderGetExtensionSupported'],
            c_args: ['-UNDEBUG'],
        )
        decor_image_probe = executable(
            'test-decor-image-x', 'test_decor_image_x.c',
            objects: fvwm3.extract_all_objects(recursive: false),
            include_directories: includedirs,
            dependencies: all_found_deps,
            link_with: libfvwm3,
            link_args: ['-Wl,--wrap=main', '-Wl,--wrap=fork'],
            c_args: ['-UNDEBUG'],
        )
        test(
            'decor-image-owner-xvfb', py,
            args: [files('live_x/picture_async_live.py'),
                '--probe', decor_image_probe.full_path(),
                '--worker', image_async_worker_fixture.full_path(),
                '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'decor-image-owner'],
            depends: [decor_image_probe, image_async_worker_fixture],
            suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
            is_parallel: false, timeout: 90,
        )
        test(
            'image-colorsets-xvfb', py,
            args: [files('live_x/image_colorsets_live.py'),
                '--fvwm', image_colorset_fixture.full_path(),
                '--mfl', fvwm_mfl.full_path(),
                '--probe', image_owner_probe.full_path(),
                '--worker', image_async_worker_fixture.full_path(),
                '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'image-colorsets'],
            depends: [image_colorset_fixture, fvwm_mfl, image_owner_probe,
                image_async_worker_fixture],
            suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
            is_parallel: false, timeout: 90,
        )
        foreach depth : [24, 8]
            test(
                'image-startup-' + depth.to_string() + '-xvfb', py,
                args: [files('live_x/image_startup_live.py'),
                    '--fvwm', fvwm3.full_path(),
                    '--mfl', fvwm_mfl.full_path(),
                    '--probe', image_owner_probe.full_path(),
                    '--worker', image_async_worker_fixture.full_path(),
                    '--depth', depth.to_string(),
                    '--artifacts-dir', meson.current_build_dir() /
                        'live-x-artifacts' / ('image-startup-' + depth.to_string())],
                depends: [fvwm3, fvwm_mfl, image_owner_probe,
                    image_async_worker_fixture],
                suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
                is_parallel: false, timeout: 90,
            )
        endforeach
        test(
            'button-release-common-xvfb', py,
            args: [files('live_x/button_release_live.py'),
                '--fvwm', fvwm3.full_path(),
                '--mfl', fvwm_mfl.full_path(),
                '--probe', image_owner_probe.full_path(),
                '--worker', image_async_worker_fixture.full_path(),
                '--timeout', get_option('image-svg-timeout').to_string(),
                '--artifacts-dir', meson.current_build_dir() /
                    'live-x-artifacts' / 'button-release-common'],
            depends: [fvwm3, fvwm_mfl, image_owner_probe,
                image_async_worker_fixture],
            suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
            is_parallel: false, timeout: get_option('image-svg-timeout') + 60,
        )
        test(
            'viewport-image-boundaries-xvfb', py,
            args: [files('live_x/button_release_live.py'), '--active-viewport',
                '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
                '--probe', image_owner_probe.full_path(),
                '--worker', image_async_worker_fixture.full_path(),
                '--timeout', get_option('image-svg-timeout').to_string(),
                '--artifacts-dir', meson.current_build_dir() /
                    'live-x-artifacts' / 'viewport-image-boundaries'],
            depends: [fvwm3, fvwm_mfl, image_owner_probe,
                image_async_worker_fixture],
            suite: 'live-x', is_parallel: false, timeout: 90,
        )
        test(
            'menu-image-boundaries-xvfb', py,
            args: [files('live_x/button_release_live.py'), '--active-menu',
                '--fvwm', fvwm3.full_path(), '--mfl', fvwm_mfl.full_path(),
                '--probe', image_owner_probe.full_path(),
                '--worker', image_async_worker_fixture.full_path(),
                '--timeout', get_option('image-svg-timeout').to_string(),
                '--artifacts-dir', meson.current_build_dir() /
                    'live-x-artifacts' / 'menu-image-boundaries'],
            depends: [fvwm3, fvwm_mfl, image_owner_probe,
                image_async_worker_fixture],
            suite: 'live-x', is_parallel: false, timeout: 90,
        )
        test(
            'image-mini-xvfb', py,
            args: [files('live_x/image_mini_live.py'),
                '--fvwm', image_colorset_fixture.full_path(),
                '--mfl', fvwm_mfl.full_path(),
                '--probe', image_owner_probe.full_path(),
                '--worker', image_async_worker_fixture.full_path(),
                '--artifacts-dir', meson.current_build_dir() /
                    'live-x-artifacts' / 'image-mini'],
            depends: [image_colorset_fixture, fvwm_mfl, image_owner_probe,
                image_async_worker_fixture],
            suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
            is_parallel: false, timeout: 90,
        )
        foreach profile : ['synthetic', 'real']
            test(
                'image-decor-' + profile + '-xvfb', py,
                args: [files('live_x/image_decor_live.py'),
                    '--fvwm', image_colorset_fixture.full_path(),
                    '--mfl', fvwm_mfl.full_path(),
                    '--worker', profile == 'real' ? fvwm_svg_worker.full_path() : image_async_worker_fixture.full_path(),
                    '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / ('image-decor-' + profile)] + (profile == 'real' ? ['--real'] : []),
                depends: [image_colorset_fixture, fvwm_mfl,
                    image_async_worker_fixture, fvwm_svg_worker],
                suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
                is_parallel: false, timeout: 90,
            )
        endforeach
        test(
            'image-menu-xvfb', py,
            args: [files('live_x/image_menu_live.py'),
                '--fvwm', image_colorset_fixture.full_path(),
                '--mfl', fvwm_mfl.full_path(),
                '--worker', image_async_worker_fixture.full_path(),
                '--artifacts-dir', meson.current_build_dir() /
                    'live-x-artifacts' / 'image-menu'],
            depends: [image_colorset_fixture, fvwm_mfl,
                image_async_worker_fixture],
            suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
            is_parallel: false, timeout: 90,
        )
    endif
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd'] and librsvg.found() and libpng.found()
        foreach depth : [24, 8]
            test(
                'image-file-icon-' + depth.to_string() + '-xvfb', py,
                args: [files('live_x/image_mini_live.py'),
                    '--fvwm', image_colorset_fixture.full_path(),
                    '--mfl', fvwm_mfl.full_path(),
                    '--probe', image_owner_probe.full_path(),
                    '--worker', image_async_worker_fixture.full_path(),
                    '--file-icon', '--depth', depth.to_string(),
                    '--artifacts-dir', meson.current_build_dir() /
                        'live-x-artifacts' / ('image-file-icon-' + depth.to_string())],
                depends: [image_colorset_fixture, fvwm_mfl, image_owner_probe,
                    image_async_worker_fixture],
                suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
                is_parallel: false, timeout: 90,
            )
        endforeach
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd'] and librsvg.found()
    root_image_probe = executable(
        'test-root-image-x', 'test_root_image_x.c',
        objects: fvwm3.extract_all_objects(recursive: false),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=main', '-Wl,--wrap=fork'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'root-image-owner-xvfb', py,
        args: [files('live_x/picture_async_live.py'),
            '--probe', root_image_probe.full_path(),
            '--worker', image_async_worker_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'root-image-owner'],
        depends: [root_image_probe, image_async_worker_fixture],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 90,
    )
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd'] and librsvg.found()
    cursor_image_link_args = ['-Wl,--wrap=main', '-Wl,--wrap=fork',
        '-Wl,--wrap=XCreatePixmapCursor']
    if xrender.found()
        cursor_image_link_args += ['-Wl,--wrap=XRenderCreateCursor']
    endif
    cursor_image_probe = executable(
        'test-cursor-image-x', 'test_cursor_image_x.c',
        objects: fvwm3.extract_all_objects(recursive: false),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: cursor_image_link_args,
        c_args: ['-UNDEBUG'],
    )
    test(
        'cursor-image-owner-xvfb', py,
        args: [files('live_x/picture_async_live.py'),
            '--probe', cursor_image_probe.full_path(),
            '--worker', image_async_worker_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'cursor-image-owner'],
        depends: [cursor_image_probe, image_async_worker_fixture],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 90,
    )
    test(
        'cursor-image-core-xvfb', py,
        args: [files('live_x/picture_async_live.py'),
            '--probe', cursor_image_probe.full_path(),
            '--worker', image_async_worker_fixture.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'cursor-image-core'],
        depends: [cursor_image_probe, image_async_worker_fixture],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 90,
        env: {'XCURSOR_CORE': '1'},
    )
endif

picture_cache_test = executable(
    'test-picture-cache', 'test_picture_cache.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=PictureFindImageFile',
        '-Wl,--wrap=isFileStampChanged'],
    c_args: ['-UNDEBUG'],
)
test('picture-cache', picture_cache_test)

# Source-RGB summary policy shared by ordinary and deferred image conversion.
picture_average_test = executable(
    'test-picture-average', 'test_picture_average.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('picture-average', picture_average_test)

# Legacy RGB16 effects and checked private worker records.
picture_color_test = executable(
    'test-picture-color', 'test_picture_color.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=PictureFreeColors'],
    c_args: ['-UNDEBUG'],
)
test('picture-color', picture_color_test)
image_effect_test = executable(
    'test-image-effect', 'test_image_effect.c',
    include_directories: includedirs,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('image-effect', image_effect_test)
image_effect_protocol_test = executable(
    'test-image-effect-protocol', 'test_image_effect_protocol.c',
    include_directories: includedirs,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=malloc'],
    c_args: ['-UNDEBUG'],
)
test('image-effect-protocol', image_effect_protocol_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    image_effect_probe = executable(
        'test-image-effect-x', 'test_image_effect_x.c',
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=FRenderGetExtensionSupported'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'image-effect-xvfb', py,
        args: [files('live_x/picture_async_live.py'),
            '--probe', image_effect_probe.full_path(),
            '--worker', fvwm_svg_worker.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'image-effect'],
        depends: [image_effect_probe, fvwm_svg_worker],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        timeout: 90, is_parallel: false,
    )
endif

if get_option('live-x-tests') and libpng.found() and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    if not librsvg.found()
        image_colorset_fixture = executable(
            'fvwm-image-colorset-fixture', 'image_colorset_start.c',
            objects: fvwm3.extract_all_objects(recursive: false),
            include_directories: includedirs,
            dependencies: all_found_deps,
            link_with: libfvwm3,
            link_args: ['-Wl,--wrap=CaptureAllWindows',
                '-Wl,--wrap=FRenderGetExtensionSupported'],
            c_args: ['-UNDEBUG'],
        )
    endif
    colorset_revision_probe = executable(
        'test-colorset-revision-x', 'test_colorset_revision_x.c',
        objects: fvwm3.extract_all_objects(recursive: false),
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        link_args: ['-Wl,--wrap=main', '-Wl,--wrap=picture_async_bind',
            '-Wl,--wrap=FRenderGetExtensionSupported'],
        c_args: ['-UNDEBUG'],
    )
    test(
        'colorset-revision-xvfb', py,
        args: [files('live_x/picture_async_live.py'),
            '--probe', colorset_revision_probe.full_path(),
            '--worker', fvwm_svg_worker.full_path(),
            '--artifacts-dir', meson.current_build_dir() /
                'live-x-artifacts' / 'colorset-revision'],
        depends: [colorset_revision_probe, fvwm_svg_worker],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        timeout: 90, is_parallel: false,
    )
    image_effect_worker_fixture = executable(
        'image-effect-worker-fixture',
        ['image_effect_worker_fixture.c', '../libs/image_effect_protocol.c',
         '../libs/image_effect.c', '../libs/ImageSafety.c'],
        include_directories: includedirs,
        override_options: ['b_sanitize=none'],
        c_args: ['-UNDEBUG'],
    )
    foreach effect_depth : [24, 8]
    test(
        effect_depth == 24 ? 'image-colorset-effects-xvfb' : 'image-colorset-effects-8bit-xvfb', py,
        args: [files('live_x/image_colorset_effects_live.py'),
            '--fvwm', image_colorset_fixture.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--probe', image_owner_probe.full_path(),
            '--worker', image_effect_worker_fixture.full_path(),
            '--real-worker', fvwm_svg_worker.full_path(),
            '--timeout', get_option('image-effect-timeout').to_string(),
            '--depth', effect_depth.to_string(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' /
                'image-colorset-effects-' + effect_depth.to_string()],
        depends: [image_colorset_fixture, fvwm_mfl, image_owner_probe,
            image_effect_worker_fixture, fvwm_svg_worker],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 40 + get_option('image-effect-timeout'),
    )
    endforeach
endif

test('auto-transport', py,
    args: [files('test_auto_transport.py'), '--module',
           meson.project_build_root() / 'modules/FvwmAuto/FvwmAuto'],
    timeout: 60,
)


script_packet_test = executable(
    'test-script-packet',
    ['test_script_packet.c', '../modules/FvwmScript/script_packet.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('script-packet', script_packet_test)


buttons_navigation_test = executable(
    'test-buttons-navigation',
    ['test_buttons_navigation.c',
     '../modules/FvwmButtons/buttons_navigation.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('buttons-navigation', buttons_navigation_test)

buttons_updates_test = executable(
    'test-buttons-updates',
    ['test_buttons_updates.c',
     '../modules/FvwmButtons/buttons_updates.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('buttons-updates', buttons_updates_test)

ui_packet_test = executable(
    'test-ui-packet', ['test_ui_packet.c', '../modules/FvwmIconMan/iconman_packet.c',
                     '../modules/FvwmButtons/buttons_packet.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('ui-packet', ui_packet_test)

pager_geometry_test = executable(
    'test-pager-geometry', ['test_pager_geometry.c', '../modules/FvwmPager/pager_geometry.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('pager-geometry', pager_geometry_test)

pager_desk_range_test = executable(
    'test-pager-desk-range',
    ['test_pager_desk_range.c', '../modules/FvwmPager/pager_desk_range.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('pager-desk-range', pager_desk_range_test)

module_monitor_test = executable(
    'test-module-monitor', 'test_module_monitor.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('module-monitor', module_monitor_test)

module_ui_scale_test = executable(
    'test-module-ui-scale', 'test_module_ui_scale.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('module-ui-scale', module_ui_scale_test)

form_scale_test = executable(
    'test-form-scale',
    ['test_form_scale.c', '../modules/FvwmForm/form_scale_policy.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('form-scale', form_scale_test)

pinboard_test = executable(
    'test-pinboard',
    ['test_pinboard.c', '../modules/FvwmPinboard/pinboard_policy.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('pinboard-policy', pinboard_test)

# A noninstalled module fixture reuses the exact production objects.  Only
# its SHAPE capability query differs; the real server remains unchanged.
pinboard_no_shape_test = executable(
    'test-pinboard-no-shape', 'pinboard_shape_unavailable.c',
    objects: fvwm_pinboard.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_compile_deps + localized_text_runtime_deps,
    link_with: libfvwm3,
    link_args: ['-Wl,--wrap=XShapeQueryExtension'],
    c_args: ['-UNDEBUG'],
    install: false,
)

pager_monitor_test = executable(
    'test-pager-monitor',
    ['test_pager_monitor.c', '../modules/FvwmPager/fpmonitor.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('pager-monitor', pager_monitor_test)

pager_packet_test = executable(
    'test-pager-packet', ['test_pager_packet.c', '../modules/FvwmPager/pager_packet.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('pager-packet', pager_packet_test)

module_x_service_test = executable(
    'test-module-x-service', ['test_module_x_service.c',
                              '../libs/module_x_service.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('module-x-service', module_x_service_test)

form_list_test = executable(
    'test-form-list', ['test_form_list.c', '../modules/FvwmForm/form_list.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('form-list', form_list_test)

form_input_test = executable(
    'test-form-input', ['test_form_input.c', '../modules/FvwmForm/form_input.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('form-input', form_input_test)


test('live-x-harness-teardown', py,
    args: [files('live_x/harness_teardown_test.py'),
           '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'harness-teardown'],
    is_parallel: false, timeout: 30)

form_command_test = executable(
    'test-form-command',
    ['test_form_command.c', '../modules/FvwmForm/ParseCommand.c',
     '../modules/FvwmForm/form_list.c',
     '../modules/FvwmForm/form_input.c'],
    include_directories: [includedirs, include_directories('../modules/FvwmForm')],
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('form-command', form_command_test)

parse_integer_test = executable(
    'test-parse-integer', 'test_parse_integer.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('parse-integer', parse_integer_test)

pager_shape_test = executable(
    'test-pager-shape',
    ['test_pager_shape.c', '../modules/FvwmPager/pager_shape.c',
     '../modules/FvwmPager/pager_geometry.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('pager-shape', pager_shape_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test(
        'script-modal-xvfb', py,
        args: [
            files('live_x/script_modal_live.py'),
            '--fvwm', fvwm3.full_path(),
            '--mfl', fvwm_mfl.full_path(),
            '--script', fvwm_script.full_path(),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'script-modal',
        ],
        depends: [fvwm3, fvwm_mfl, fvwm_script],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        timeout: 180, is_parallel: false,
    )
endif

font_text_test = executable(
    'test-font-text',
    'test_font_text.c',
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('font-text', font_text_test)

config_line_test = executable(
    'test-config-line', 'test_config_line.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('config-line', config_line_test)


if get_option('live-x-tests')
    test('desktop-menu-xvfb', py,
        args: [files('live_x/xdg_menu_live.py'),
            '--fvwm', fvwm3,
            '--mfl', fvwm_mfl,
            '--form', fvwm_form,
            '--provider', fvwm_menu_desktop,
            '--artifacts-dir', meson.current_build_dir() / 'live-x-desktop-menu'],
        is_parallel: false,
        timeout: 120,
    )
endif

parse_token_test = executable(
    'test-parse-token', 'test_parse_token.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('parse-token', parse_token_test)

test('ci-asset-contract', py, args: [files('test_ci_assets.py')],
    timeout: 30)
test('ci-png-contract', py, args: [files('test_ci_png.py')],
    timeout: 30)
test('qemu-bsd-contract', py, args: [files('test_qemu_bsd.py')],
    timeout: 60)

# Client snapshots own IDs only; allocation refusal leaves their prefix intact.
ewmh_state_policy_test = executable(
    'test-ewmh-state-policy',
    ['test_ewmh_state_policy.c', '../fvwm/ewmh_client_list.c',
     '../fvwm/ewmh_atom_index.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    c_args: ['-UNDEBUG'],
    link_args: ['-Wl,--wrap=malloc', '-Wl,--wrap=realloc', '-Wl,--wrap=free'],
)
test('ewmh-state-policy', ewmh_state_policy_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    foreach lifetime_case : ['selection', 'classification', 'wait']
        test('function-lifetime-' + lifetime_case + '-xvfb', py,
            args: [files('live_x/function_lifetime_live.py'),
                '--fvwm', fvwm3, '--mfl', fvwm_mfl,
                '--case', lifetime_case,
                '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts/function-lifetime' / lifetime_case],
            suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
            is_parallel: false, timeout: 45,
        )
    endforeach
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    foreach manager_case : ['announcement', 'replacement', 'failure']
        test('manager-handoff-' + manager_case + '-xvfb', py,
            args: [files('live_x/manager_handoff_live.py'),
                '--fvwm', fvwm3, '--mfl', fvwm_mfl,
                '--case', manager_case,
                '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts/manager-handoff' / manager_case],
            suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
            is_parallel: false, timeout: 90,
        )
    endforeach
endif

ewmh_desktop_policy_test = executable(
    'test-ewmh-desktop-policy', 'test_ewmh_desktop_policy.c',
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('ewmh-desktop-policy', ewmh_desktop_policy_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    foreach desktop_case : ['count', 'current', 'arrays']
        test('desktop-publication-' + desktop_case + '-xvfb', py,
            args: [files('live_x/desktop_publication_live.py'),
                '--fvwm', fvwm3, '--mfl', fvwm_mfl,
                '--case', desktop_case,
                '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts/desktop-publication' / desktop_case],
            suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
            is_parallel: false, timeout: 90,
        )
    endforeach
endif


ewmh_workarea_policy_test = executable(
    'test-ewmh-workarea-policy', 'test_ewmh_workarea_policy.c',
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('ewmh-workarea-policy', ewmh_workarea_policy_test)

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    foreach workarea_case : ['publication', 'revocation', 'bounds']
        test('workarea-' + workarea_case + '-xvfb', py,
            args: [files('live_x/workarea_live.py'),
                '--fvwm', fvwm3, '--mfl', fvwm_mfl,
                '--case', workarea_case,
                '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts/workarea' / workarea_case],
            suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
            is_parallel: false, timeout: 90,
        )
    endforeach
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    foreach manager_case : ['values', 'multiple', 'admission']
        test('manager-targets-' + manager_case + '-xvfb', py,
            args: [files('live_x/manager_targets_live.py'),
                '--fvwm', fvwm3, '--mfl', fvwm_mfl,
                '--case', manager_case,
                '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts/manager-targets' / manager_case],
            suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
            is_parallel: false, timeout: 60,
        )
    endforeach
endif

if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    if libpng.found()
        test('action-palette-xvfb', py,
            args: [files('live_x/action_palette_live.py'),
                '--build', meson.project_build_root(),
                '--artifacts-dir', meson.project_build_root() / 'live-x-action-palette'],
            depends: [fvwm3, fvwm_mfl, fvwm_form, fvwm_desktop,
                fvwm_desktop_worker, fvwm_desktop_capture,
                fvwm_backer, fvwm_buttons, fvwm_event],
            timeout: 480,
            is_parallel: false,
        )
        test('native-bindings-xvfb', py,
            args: [files('live_x/native_bindings_live.py'),
                '--build', meson.project_build_root(),
                '--artifacts-dir', meson.project_build_root() / 'live-x-native-bindings'],
            depends: [fvwm3, fvwm_mfl, fvwm_form, fvwm_desktop,
                fvwm_desktop_worker, fvwm_file_browser, fvwmcommand,
                fvwm_desktop_capture, fvwm_backer, fvwm_buttons, fvwm_event],
            timeout: 480,
            is_parallel: false,
        )
        test('pinboard-xvfb', py,
            args: [files('live_x/pinboard_live.py'),
                '--build', meson.project_build_root(),
                '--artifacts-dir', meson.project_build_root() / 'live-x-pinboard'],
            depends: [fvwm3, fvwm_mfl, fvwm_form, fvwm_pinboard,
                fvwm_pinboard_owner, fvwm_file_browser, fvwm_desktop,
                fvwm_desktop_worker, fvwm_desktop_capture,
                fvwm_backer, fvwm_buttons, fvwm_event],
            timeout: 480,
            is_parallel: false,
        )
    endif
    test('file-browser-xvfb', py,
        args: [files('live_x/file_browser_live.py'),
            '--fvwm', fvwm3, '--mfl', fvwm_mfl,
            '--form', fvwm_form, '--browser', fvwm_file_browser,
            '--command-client', fvwmcommand,
            '--artifacts-dir', meson.project_build_root() / 'live-x-file-browser'],
        timeout: 90,
        is_parallel: false,
    )
endif

capture_policy_test = executable(
    'test-capture-policy',
    ['test_capture_policy.c', '../libs/capture_policy.c', '../libs/ImageSafety.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('capture-policy', capture_policy_test)
if libpng.found() and get_option('live-x-tests')
    test('desktop-capture-xvfb', py,
        args: [files('live_x/capture_live.py'),
            '--capture', fvwm_desktop_capture,
            '--artifacts-dir', meson.project_build_root() / 'live-x-capture'],
        timeout: 90,
        is_parallel: false,
    )
endif

# Observe the actual module owners; the record test also runs with LSan enabled.
iconman_record_owner_test = executable(
    'test-iconman-record-owner', 'test_iconman_record_owner.c',
    objects: fvwm_iconman.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG', '-fno-builtin-free'],
    link_args: ['-Wl,--wrap=main', '-Wl,--wrap=free'],
)
test('iconman-record-owner', iconman_record_owner_test)

ident_resource_owner_probe = executable(
    'test-ident-resource-owner', 'test_ident_resource_owner.c',
    objects: fvwm_ident.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG', '-fno-builtin-free'],
    link_args: ['-Wl,--wrap=main', '-Wl,--wrap=free',
        '-Wl,--wrap=CopyString', '-Wl,--wrap=CopyStringWithQuotes',
        '-Wl,--wrap=fxstrdup', '-Wl,--wrap=FlocaleAllocateWinString',
        '-Wl,--wrap=FlocaleTextWidth', '-Wl,--wrap=fxmalloc',
        '-Wl,--wrap=FlocaleLoadFont', '-Wl,--wrap=FlocaleUnloadFont',
        '-Wl,--wrap=fvwmlib_XCreateGC', '-Wl,--wrap=XFreeGC',
        '-Wl,--wrap=fvwmlib_get_color', '-Wl,--wrap=GetColor',
        '-Wl,--wrap=PictureFreeColors', '-Wl,--wrap=XMapWindow',
        '-Wl,--wrap=monitor_by_output',
        '-Wl,--wrap=fvwmlib_get_target_window'],
)
buttons_picture_owner_probe = executable(
    'test-buttons-picture-owner', 'test_buttons_picture_owner.c',
    objects: fvwm_buttons.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
    link_args: ['-Wl,--wrap=main', '-Wl,--wrap=PCacheFvwmPicture',
        '-Wl,--wrap=PDestroyFvwmPicture', '-Wl,--wrap=XMapWindow'],
)
if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('module-resource-owner-xvfb', py,
        args: [files('live_x/module_resource_owner_live.py'),
            '--ident', ident_resource_owner_probe,
            '--buttons', buttons_picture_owner_probe,
            '--fixture', ident_packet_fixture,
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'module-resource-owner'],
        depends: [ident_resource_owner_probe, buttons_picture_owner_probe,
            ident_packet_fixture],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 120)
endif

# Link the same module objects used by the installed executables. The pure
# Event record test runs with leak detection; actual-X observers assert only
# selected owners because shared graphics/font caches have separate lifetimes.
event_action_owner_test = executable(
    'test-event-action-owner', 'test_event_action_owner.c',
    objects: fvwm_event.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG', '-fno-builtin-free'],
    link_args: ['-Wl,--wrap=main', '-Wl,--wrap=free',
        '-Wl,--wrap=fxmalloc', '-Wl,--wrap=fxstrdup',
        '-Wl,--wrap=GetNextSimpleOption', '-Wl,--wrap=ParseModuleArgs',
        '-Wl,--wrap=InitGetConfigLine', '-Wl,--wrap=GetConfigLine',
        '-Wl,--wrap=ReadFvwmPacket', '-Wl,--wrap=SendText',
        '-Wl,--wrap=SetMessageMask', '-Wl,--wrap=SetSyncMask',
        '-Wl,--wrap=SendFinishedStartupNotification',
        '-Wl,--wrap=SendUnlockNotification'],
)
test('event-action-owner', event_action_owner_test)

pager_drawing_owner_probe = executable(
    'test-pager-drawing-owner', 'test_pager_drawing_owner.c',
    objects: fvwm_pager.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG', '-fno-builtin-free'],
    link_args: ['-Wl,--wrap=main', '-Wl,--wrap=free',
        '-Wl,--wrap=PCacheFvwmPicture', '-Wl,--wrap=PCloneFvwmPicture',
        '-Wl,--wrap=PDestroyFvwmPicture', '-Wl,--wrap=XFreeGC',
        '-Wl,--wrap=module_packet_reader_init',
        '-Wl,--wrap=SendFinishedStartupNotification'],
)
animate_drawing_owner_probe = executable(
    'test-animate-drawing-owner', 'test_animate_drawing_owner.c',
    objects: fvwm_animate.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG', '-fno-builtin-free'],
    link_args: ['-Wl,--wrap=main', '-Wl,--wrap=free',
        '-Wl,--wrap=InitGetConfigLine', '-Wl,--wrap=PGetFvwmPicture',
        '-Wl,--wrap=XCreatePixmap', '-Wl,--wrap=XFreePixmap',
        '-Wl,--wrap=fvwmlib_XCreateGC', '-Wl,--wrap=XFreeGC',
        '-Wl,--wrap=SendText', '-Wl,--wrap=SendUnlockNotification',
        '-Wl,--wrap=SendFinishedStartupNotification'],
)
if get_option('live-x-tests') and host_machine.system() in ['linux', 'freebsd', 'openbsd']
    test('module-drawing-owner-xvfb', py,
        args: [files('live_x/module_drawing_owner_live.py'),
            '--pager', pager_drawing_owner_probe,
            '--animate', animate_drawing_owner_probe,
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'module-drawing-owner'],
        depends: [pager_drawing_owner_probe, animate_drawing_owner_probe],
        suite: host_machine.system() == 'linux' ? 'live-x' : 'bsd-x',
        is_parallel: false, timeout: 120)
endif

ui_layout_test = executable(
    'test-ui-layout', ['test_ui_layout.c', '../libs/ui_layout.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)
test('ui-layout', ui_layout_test)

config_origin_test = executable(
    'test-config-origin', 'test_config_origin.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
test('config-origin', config_origin_test)

# Native consumer of the finalized monitor UI handoff; never installed.
module_ui_scale_probe = executable(
    'module-ui-scale-probe', 'module_ui_scale_probe.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)
if get_option('live-x-tests') and host_machine.system() == 'linux'
    test('module-ui-scale-xorg-dummy', py,
        args: [files('live_x/module_ui_scale_live.py'),
            '--build', meson.project_build_root(),
            '--probe', module_ui_scale_probe,
            '--xorg-config', files('live_x/xorg-dummy.conf'),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'module-ui-scale'],
        depends: [fvwm3, fvwm_mfl, module_ui_scale_probe],
        suite: 'live-x', is_parallel: false, timeout: 180,
    )
endif

# Test-only acquisition refusal with the production Form event/ownership paths.
form_scale_refuse_probe = executable(
    'form-scale-refuse-probe', 'form_scale_refuse_probe.c',
    objects: fvwm_form.extract_all_objects(recursive: false),
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
    link_args: ['-Wl,--wrap=FlocaleLoadFontAtScale'],
)

if get_option('live-x-tests') and host_machine.system() == 'linux' and libpng.found()
    test('viewport-scale-xorg-dummy', py,
        args: [files('live_x/viewport_scale_live.py'),
            '--build', meson.project_build_root(),
            '--fault-form', form_scale_refuse_probe,
            '--xorg-config', files('live_x/xorg-dummy.conf'),
            '--artifacts-dir', meson.current_build_dir() / 'live-x-artifacts' / 'viewport-scale'],
        depends: [fvwm3, fvwm_mfl, fvwm_form, fvwm_file_browser,
            fvwm_desktop, fvwm_desktop_worker, fvwm_backer, fvwm_buttons,
            fvwm_event, form_scale_refuse_probe, fvwm_desktop_capture],
        suite: 'live-x', is_parallel: false, timeout: 360,
    )
endif
