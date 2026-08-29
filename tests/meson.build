image_safety_test = executable(
    'test-image-safety',
    ['test_image_safety.c', '../libs/ImageSafety.c'],
    include_directories: include_directories('../libs'),
    c_args: ['-UNDEBUG'],
)

test('image-safety', image_safety_test)

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
