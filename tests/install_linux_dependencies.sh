#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# Ubuntu build/X test dependencies shared by native CI and local containers.
set -eu
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y build-essential clang lld ninja-build pkg-config \
    python3 python3-venv perl libfontconfig1-dev libice-dev libevent-dev \
    libx11-dev libxext-dev libxft-dev libxrandr-dev libxt-dev xtrans-dev \
    libxkbcommon-dev libfreetype6-dev libfribidi-dev libpng-dev librsvg2-dev \
    libcairo2-dev libsm-dev libxcursor-dev libxfixes-dev libxrender-dev \
    libxpm-dev gettext xvfb x11-utils x11-xserver-utils xserver-xorg-core \
    xserver-xorg-video-dummy fonts-dejavu-core libxtst6
