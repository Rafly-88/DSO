#!/usr/bin/env bash
# Build script for DSO on standard x86_64 Ubuntu (desktop / laptop / server).
# Run from the project root (the directory with CMakeLists.txt).
#
# Usage:
#   chmod +x build_ubuntu.sh
#   ./build_ubuntu.sh
#
# Output goes to ./build_ubuntu/ so it never collides with ./build_arm/
# produced by build_jetson.sh.
#
# If you get "Permission denied" or "command not found", make it executable:
#   chmod +x build_ubuntu.sh
# Alternatively run it explicitly with bash:
#   bash build_ubuntu.sh

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

echo "==> Verifying we are on x86_64"
ARCH="$(uname -m)"
if [ "$ARCH" != "x86_64" ]; then
    echo "ERROR: This script is for x86_64 Ubuntu. Detected: $ARCH"
    echo "       For ARM / Jetson, use ./build_jetson.sh instead."
    exit 1
fi

echo "==> Installing system dependencies via apt (must be first — fresh Ubuntu has no git/cmake)"
sudo apt-get update
# DSO only needs Boost system+thread (see CMakeLists.txt), not the whole
# libboost-all-dev metapackage which pulls libboost-python1.74-dev and
# triggers python3-dev conflicts on fresh Ubuntu installs.
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libsuitesparse-dev \
    libeigen3-dev \
    libboost-system-dev \
    libboost-thread-dev \
    libopencv-dev \
    libgl1-mesa-dev \
    libglew-dev \
    libegl1-mesa-dev \
    libwayland-dev \
    libxkbcommon-dev \
    wayland-protocols \
    zlib1g-dev \
    libavcodec-dev \
    libavutil-dev \
    libavformat-dev \
    libswscale-dev \
    libavdevice-dev

echo "==> Initializing git submodules"
if [ -d .git ]; then
    git submodule update --init --recursive || true
fi

echo "==> Checking for Pangolin"
if ! pkg-config --exists pangolin \
   && [ ! -f /usr/local/lib/cmake/Pangolin/PangolinConfig.cmake ] \
   && [ ! -f /usr/lib/cmake/Pangolin/PangolinConfig.cmake ]; then
    echo "==> Pangolin not found. Building Pangolin from source"
    PANGOLIN_DIR="$HOME/Pangolin"
    if [ ! -d "$PANGOLIN_DIR" ]; then
        git clone --recursive --branch v0.9.1 https://github.com/stevenlovegrove/Pangolin.git "$PANGOLIN_DIR"
    fi
    cd "$PANGOLIN_DIR"
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_PANGOLIN_PYTHON=OFF
    # Limit parallel jobs to avoid OOM crashes on memory-tight machines.
    PANGOLIN_JOBS="${JOBS:-$(( $(nproc) / 2 ))}"
    [ "$PANGOLIN_JOBS" -lt 1 ] && PANGOLIN_JOBS=1
    make -j"$PANGOLIN_JOBS"
    sudo make install
    sudo ldconfig
    cd "$PROJECT_DIR"
else
    echo "==> Pangolin already installed"
fi

echo "==> Configuring DSO with CMake (x86_64 Ubuntu)"
BUILD_DIR="build_ubuntu"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# CMakeLists.txt auto-detects x86_64 and uses the toolchain's native SSE/AVX
# headers (no sse2neon shim).
cmake .. -DCMAKE_BUILD_TYPE=Release

echo "==> Building DSO"
# Limit parallel jobs to avoid OOM crashes (template-heavy Eigen/Sophus code).
# Default: half of available cores. Override with: JOBS=4 ./build_ubuntu.sh
DSO_JOBS="${JOBS:-$(( $(nproc) / 2 ))}"
[ "$DSO_JOBS" -lt 1 ] && DSO_JOBS=1
echo "    Using $DSO_JOBS parallel jobs (override with JOBS=N)"
make -j"$DSO_JOBS"

echo ""
echo "==> Build complete."
echo "    Library: $PROJECT_DIR/$BUILD_DIR/lib/libdso.a"
echo "    Binaries: $PROJECT_DIR/$BUILD_DIR/bin/"
ls -lh "$PROJECT_DIR/$BUILD_DIR/bin/" 2>/dev/null || true
