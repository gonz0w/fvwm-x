image_safety_test = executable(
    'test-image-safety',
    ['test_image_safety.c', '../libs/ImageSafety.c'],
    include_directories: include_directories('../libs'),
    c_args: ['-UNDEBUG'],
)

test('image-safety', image_safety_test)

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

monitor_ownership_test = executable(
    'test-monitor-ownership',
    'test_monitor_ownership.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('monitor-ownership', monitor_ownership_test)

monitor_scale_config_test = executable(
    'test-monitor-scale-config',
    ['test_monitor_scale_config.c', '../fvwm/monitor_scale_config.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG', '-DMONITOR_SCALE_CONFIG_TESTING'],
)

test('monitor-scale-config', monitor_scale_config_test)

status_json_test = executable(
    'test-status-json',
    ['test_status_json.c', '../fvwm/status_json.c'],
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('status-json', status_json_test)

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
)

test('module-runtime', module_runtime_test)

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
    wrapped_fvwm_sources = []
    foreach source : fvwm_sources
        wrapped_fvwm_sources += '../' / source
    endforeach
    fvwm3_live_x_wrap = executable(
        'fvwm3-live-x-wrap',
        [wrapped_fvwm_sources, 'live_x/xrr_query_fault.c'],
        include_directories: includedirs,
        dependencies: all_found_deps,
        link_with: libfvwm3,
        c_args: ['-DFVWM_XRR_QUERY_FAULT_WRAP'],
        link_args: ['-Wl,--wrap=XRRGetMonitors'],
        install: false,
    )

    live_x_artifacts = meson.current_build_dir() / 'live-x-artifacts'
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
        suite: 'live-x',
        is_parallel: false,
        timeout: 60,
    )
endif
