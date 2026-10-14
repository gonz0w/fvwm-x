#!/bin/sh

set -eu

# Keep config.h's timestamp when reconfiguration produced identical bytes;
# otherwise every source including it is needlessly rebuilt.
config_tmp=$(mktemp "${3}.XXXXXX")
trap 'rm -f "$config_tmp"' EXIT HUP INT TERM
cat "$1" "$2" > "$config_tmp"
if ! cmp -s "$config_tmp" "$3"; then
    mv "$config_tmp" "$3"
fi
