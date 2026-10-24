#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.

# Focused utility acceptance after native setup. The compiler is required in
# every profile; this selector stages and exercises the installed consumers.
set -eu
build_dir=$1
python_bin=$2
meson compile -C "$build_dir" FvwmPrompt-test-binaries FvwmPrompt FvwmCommand fvwm-menu-xlock fvwm-menu-directory fvwm-script-ComExample fvwm-menu-desktop fvwm3:executable FvwmMFL FvwmScript FvwmForm -j 2
meson test -C "$build_dir" --no-rebuild --print-errorlogs \
    prompt-transport prompt-client command-client \
    prompt-protocol-xvfb command-client-xvfb \
    xlock-menu-contracts xlock-menu xlock-menu-xvfb \
    directory-menu-contracts directory-menu directory-menu-xvfb \
    script-com-example script-com-example-xvfb desktop-menu desktop-menu-xvfb
stage_dir=$(cd "$build_dir" && pwd)/install-stage
meson install -C "$build_dir" --no-rebuild \
    --tags prompt-client,command-client,xlock-menu,directory-menu,script-com-example,desktop-menu --destdir "$stage_dir"
"$python_bin" tests/test_prompt_client.py \
    --prompt "$stage_dir/usr/local/bin/FvwmPrompt"
"$python_bin" tests/test_command_client.py \
    --command "$stage_dir/usr/local/bin/FvwmCommand"
xlock_version=$("$stage_dir/usr/local/bin/fvwm-menu-xlock" --version)
"$python_bin" tests/test_utility_contracts.py \
    --xlock "$stage_dir/usr/local/bin/fvwm-menu-xlock" \
    --xlock-version "$xlock_version"
"$python_bin" tests/test_xlock_menu.py \
    --xlock "$stage_dir/usr/local/bin/fvwm-menu-xlock"
directory_version=$("$stage_dir/usr/local/bin/fvwm-menu-directory" --version)
"$python_bin" tests/test_utility_contracts.py \
    --directory "$stage_dir/usr/local/bin/fvwm-menu-directory" \
    --directory-version "$directory_version"
"$python_bin" tests/test_directory_menu.py \
    --directory "$stage_dir/usr/local/bin/fvwm-menu-directory"
"$python_bin" tests/test_script_com_example.py \
    --helper "$stage_dir/usr/local/share/fvwm3/fvwm-script-ComExample"
"$python_bin" tests/test_xdg_desktop.py \
    --provider "$stage_dir/usr/local/bin/fvwm-menu-desktop"
if [ "$(uname -s)" = Linux ]; then
    # Execute the concurrent client, not just pure units compiled with -race.
    build_abs=$(cd "$build_dir" && pwd)
    go test -C bin/FvwmPrompt -mod=vendor -race ./...
    go vet -C bin/FvwmPrompt -mod=vendor ./...
    go build -C bin/FvwmPrompt -mod=vendor -race \
        -o "$build_abs/FvwmCommand-race" ./cmd/fvwmcommand
    "$python_bin" tests/test_command_client.py \
        --command "$build_abs/FvwmCommand-race"
    "$python_bin" tests/live_x/command_client_live.py \
        --fvwm "$build_abs/fvwm3" \
        --mfl "$build_abs/modules/FvwmMFL/FvwmMFL" \
        --command "$build_abs/FvwmCommand-race" \
        --artifacts-dir "$build_abs/live-x-command-race"
fi
echo "Go client build, tests, and staged installation passed: $stage_dir"
