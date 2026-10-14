#!/bin/sh

# Find all Go source files in the current directory and its subdirectories.
# That is all

find "$1" -type f -name '*.go' | LC_ALL=C sort
