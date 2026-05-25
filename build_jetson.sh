#!/usr/bin/env bash
# Build script for DSO on Jetson Orin Nano (aarch64, JetPack 6 / Ubuntu 22.04).
# Run this ON THE JETSON, from the project root (the directory with CMakeLists.txt).
#
# Usage:
#   chmod +x build_jetson.sh
#   ./build_jetson.sh
#
# If you get "Permission denied" or "command not found", make it executable:
#   chmod +x build_jetson.sh
# Alternatively run it explicitly with bash:
#   bash build_jetson.sh

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

echo "==> Verifying we are on aarch64"
ARCH="$(uname -m)"
if [ "$ARCH" != "aarch64" ]; then
    echo "ERROR: This script is for aarch64 (Jetson). Detected: $ARCH"
    exit 1
fi

echo "==> Installing system dependencies via apt (must be first — fresh Jetson may lack git/cmake)"
sudo apt-get update
# DSO only needs Boost system+thread (see CMakeLists.txt), not the whole
# libboost-all-dev metapackage which can pull libboost-python and trigger
# python3-dev conflicts.
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \s
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

echo "==> Initializing git submodules (sse2neon)"
if [ -d .git ]; then
    git submodule update --init --recursive
fi
if [ ! -f thirdparty/sse2neon/SSE2NEON.h ]; then
    echo "ERROR: thirdparty/sse2neon is empty. Run 'git submodule update --init' manually."
    exit 1
fi

echo "==> Checking for Pangolin"
if ! pkg-config --exists pangolin && [ ! -f /usr/local/lib/cmake/Pangolin/PangolinConfig.cmake ] && [ ! -f /usr/lib/cmake/Pangolin/PangolinConfig.cmake ]; then
    echo "==> Pangolin not found. Building Pangolin from source"
    PANGOLIN_DIR="$HOME/Pangolin"
    if [ ! -d "$PANGOLIN_DIR" ]; then
        git clone --recursive --branch v0.9.1 https://github.com/stevenlovegrove/Pangolin.git "$PANGOLIN_DIR"
    fi
    cd "$PANGOLIN_DIR"
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_PANGOLIN_PYTHON=OFF
    # Limit parallel jobs to avoid OOM crashes on the Jetson (8GB RAM).
    PANGOLIN_JOBS="${JOBS:-$(( $(nproc) / 2 ))}"
    [ "$PANGOLIN_JOBS" -lt 1 ] && PANGOLIN_JOBS=1
    make -j"$PANGOLIN_JOBS"
    sudo make install
    sudo ldconfig
    cd "$PROJECT_DIR"
else
    echo "==> Pangolin already installed"
fi

echo "==> Configuring DSO with CMake (ARM/Jetson)"
BUILD_DIR="build_arm"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# -march=native on Jetson aarch64 GCC detects the Cortex-A78AE correctly.
# We do NOT override CXX flags here so the in-tree CMakeLists optimizations apply.
# CMakeLists.txt auto-detects aarch64 and enables the sse2neon include path.
cmake .. -DCMAKE_BUILD_TYPE=Release

echo "==> Building DSO (this can take a while on Jetson)"
# Limit parallel jobs to avoid OOM crashes (template-heavy Eigen/Sophus code).
# Default: half of available cores. Override with: JOBS=4 ./build_jetson.sh
DSO_JOBS="${JOBS:-$(( $(nproc) / 2 ))}"
[ "$DSO_JOBS" -lt 1 ] && DSO_JOBS=1
echo "    Using $DSO_JOBS parallel jobs (override with JOBS=N)"
make -j"$DSO_JOBS"

echo ""
echo "==> Build complete."
echo "    Library: $PROJECT_DIR/$BUILD_DIR/lib/libdso.a"
echo "    Binaries: $PROJECT_DIR/$BUILD_DIR/bin/"
ls -lh "$PROJECT_DIR/$BUILD_DIR/bin/" 2>/dev/null || true
