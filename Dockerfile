FROM fvwmorg/fvwm3-build:latest
#FROM docker.pkg.github.com/fvwmorg/fvwm3/fvwm3-build:latest

ENV GOROOT="/usr/lib/go/"
ENV PATH="$GOROOT/bin:$PATH"
ENV GOTOOLCHAIN="local"
ENV GOPROXY="off"
ENV GOSUMDB="off"

COPY . /build
WORKDIR /build

RUN python3 tools/maintenance/go_version.py "$(go env GOVERSION)" && meson setup --reconfigure build -Dmandoc=true && meson compile -C build
