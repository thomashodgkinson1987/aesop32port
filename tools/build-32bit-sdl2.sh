#!/bin/sh
# build-32bit-sdl2.sh
#
# Reproduces the vendored 32-bit SDL2 in ../SDL2_32 (SDL2 2.30.0) with a
# WORKING video backend. This is the configuration proven on CachyOS
# (2026-09): X11 video via XWayland, OpenGL renderer, ALSA audio loaded
# dynamically at runtime.
#
# History: the original vendored build (made on a different machine) was
# configured with only the offscreen/dummy video drivers — SDL_Init and
# SDL_CreateWindow "succeeded" but no window could ever appear. It was
# rebuilt with the flags below to fix that. Do NOT drop the
# --disable-video-x11-xcursor/--disable-video-x11-xrandr flags: the
# corresponding 32-bit libs (lib32-libxcursor, lib32-libxrandr) may not be
# installed, and the engine does not need them.
#
# Requirements (Arch/CachyOS package names):
#   - multilib-devel                (32-bit gcc)
#   - lib32-libx11 lib32-libxext lib32-libxfixes   (X11 backend)
#   - lib32-mesa                    (OpenGL renderer)
#   - lib32-alsa-lib (optional)     (ALSA audio, loaded dynamically)
# Not required: lib32-libxcursor, lib32-libxrandr (disabled below).
set -e

SDL2_VERSION=2.30.0
HERE=$(cd "$(dirname "$0")" && pwd)          # <repo>/tools
REPO_ROOT=$(dirname "$HERE")                 # <repo>
PREFIX=$(dirname "$REPO_ROOT")/SDL2_32       # sibling of the repo — matches the Makefile's default SDL2_DIR

cd "$REPO_ROOT"

# 1. Fetch and unpack the upstream source (or keep a local copy of the tarball).
if [ ! -d "SDL2-$SDL2_VERSION" ]; then
    curl -LO "https://libsdl.org/release/SDL2-$SDL2_VERSION.tar.gz"
    tar xzf "SDL2-$SDL2_VERSION.tar.gz"
fi

cd "SDL2-$SDL2_VERSION"

# 2. Configure. CC/CXX must carry -m32 explicitly — configure will happily
#    produce a 64-bit library otherwise (autoconf treats i686-on-x86_64 as
#    non-cross since the binaries still run).
./configure \
    --prefix="$PREFIX" \
    --host=i686-linux-gnu \
    CC="gcc -m32" CXX="g++ -m32" \
    --disable-video-x11-xinput \
    --disable-video-x11-xcursor \
    --disable-video-x11-xrandr \
    --disable-video-x11-scrnsaver \
    --disable-video-x11-vm \
    --disable-oss \
    --disable-pipewire \
    --disable-jack

# 3. Build and install into ../SDL2_32 (bin/, include/, lib/, share/).
make -j"$(nproc)"
make install

echo
echo "Done. 32-bit SDL2 (X11 + OpenGL + ALSA-dynamic) installed to: $PREFIX"
echo "The repo Makefile picks it up automatically via SDL2_DIR=../SDL2_32."
