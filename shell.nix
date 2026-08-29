{pkgs ? import <nixpkgs> {}}:
pkgs.mkShell {
  nativeBuildInputs = with pkgs.buildPackages; [
    asciidoctor
    fontconfig
    freetype
    fribidi
    go
    gnused
    libevent
    libice
    libpng
    librsvg
    libsm
    libx11
    libxcursor
    libxext
    libxfixes
    libxft
    libxi
    libxkbcommon
    libxpm
    libxrandr
    libxrender
    libxt
    meson
    ncurses
    ninja
    perl
    pkg-config
    python3
    readline
    sharutils
    xtrans
  ];
}
