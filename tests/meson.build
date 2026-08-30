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
