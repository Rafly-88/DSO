# DSO Installation Guide (Direct Sparse Odometry)

A DSO implementation with integrated Pangolin shader support for fisheye
visualization. Two build targets are supported, with **fully separated**
output (artifacts live in different directories so they never overwrite
each other):

| Target           | Build script        | Output directory  |
| ---------------- | ------------------- | ----------------- |
| ARM / Jetson     | `build_jetson.sh`   | `build_arm/`      |
| Ubuntu x86_64    | `build_ubuntu.sh`   | `build_ubuntu/`   |

`CMakeLists.txt` auto-detects the architecture (`CMAKE_SYSTEM_PROCESSOR`)
and enables the right flags:

- **aarch64 / arm** → includes `thirdparty/sse2neon` (SSE → NEON shim)
- **x86_64** → uses the toolchain's native SSE/AVX headers + `-msse4.2`

---

## Quick Start

To just get going on Ubuntu x86_64:

```bash
git clone --recursive https://github.com/Rafly-88/DSO.git
cd DSO
chmod +x build_ubuntu.sh
./build_ubuntu.sh
```

When the build finishes, binaries are in `build_ubuntu/bin/`. The rest of
this document covers the details.

---

## 1. Prerequisites

### Hardware

| Target           | Minimum                                            |
| ---------------- | -------------------------------------------------- |
| Jetson / ARM     | Jetson Orin Nano 8GB (or equivalent aarch64 board) |
| Ubuntu x86_64    | CPU with SSE4.2, ≥ 4GB RAM (8GB recommended)       |

### Operating System

- Ubuntu 20.04 / 22.04 / 24.04 (x86_64)
- Ubuntu 22.04 on Jetson (JetPack 6.x)

### Main Dependencies

The build scripts install all of these automatically via `apt`:

- `build-essential`, `cmake`, `git`, `pkg-config`
- `libsuitesparse-dev`, `libeigen3-dev`
- `libboost-system-dev`, `libboost-thread-dev`
  (DSO only needs these two Boost components — **do not** install
  `libboost-all-dev`, which pulls in `libboost-python1.74-dev` and conflicts
  with `python3-dev` on fresh Ubuntu installs)
- `libopencv-dev`
- `libgl1-mesa-dev`, `libglew-dev`, `libegl1-mesa-dev`
- `libwayland-dev`, `libxkbcommon-dev`, `wayland-protocols`
- `libavcodec-dev`, `libavutil-dev`, `libavformat-dev`, `libswscale-dev`, `libavdevice-dev`

**Pangolin** (v0.9.1) is automatically cloned and built from source if it's
not already detected on the system (installed to `/usr/local`).

---

## 2. Clone the Repository

The repo uses a git submodule (`thirdparty/sse2neon`), so clone with
`--recursive`:

```bash
git clone --recursive https://github.com/Rafly-88/DSO.git
cd DSO
```

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

> **Note**: if your fresh system doesn't have `git` yet, don't worry — the
> build script installs `git` via apt first, then runs `git submodule update`.
> So you can just download the ZIP from GitHub and run the build script.

---

## 3. Build (Automatic — recommended)

### On Ubuntu x86_64

```bash
chmod +x build_ubuntu.sh
./build_ubuntu.sh
```

Output → `build_ubuntu/bin/` and `build_ubuntu/lib/libdso.a`

### On Jetson / ARM (aarch64)

```bash
chmod +x build_jetson.sh
./build_jetson.sh
```

Output → `build_arm/bin/` and `build_arm/lib/libdso.a`

### What to do if `./build_*.sh` fails to run

Three equivalent ways to invoke the script:

```bash
chmod +x build_ubuntu.sh && ./build_ubuntu.sh   # standard
bash build_ubuntu.sh                            # no chmod needed
sh   build_ubuntu.sh                            # also works
```

### ⚠️ DO NOT run the script with `sudo`

```bash
# ❌ DON'T
sudo ./build_ubuntu.sh

# ✅ DO
./build_ubuntu.sh
```

The script already calls `sudo apt-get install` internally where needed.
If you run the whole script with `sudo`:

- `$HOME` becomes `/root` → Pangolin will be cloned into `/root/Pangolin`
- The `build_ubuntu/` directory will be owned by root → hard to delete without sudo

Run it as a regular user; sudo will prompt for the password once for `apt`.

### Controlling the number of compile threads

By default both scripts use **half** of the available CPU cores to avoid OOM
(Eigen/Sophus templates are very memory-hungry during compilation). Override
with the `JOBS=N` env var:

```bash
JOBS=4 ./build_ubuntu.sh   # use 4 threads
JOBS=2 ./build_jetson.sh   # safest when RAM is tight
JOBS=1 ./build_jetson.sh   # serial — slowest, but most stable
```

### What the script does

1. Verifies the host architecture (fails fast if you ran the wrong script)
2. `apt-get update` and installs **all** dependencies (including `git`, `cmake`)
3. `git submodule update --init --recursive` for `thirdparty/sse2neon`
4. Builds & installs Pangolin v0.9.1 from source if missing
5. Configures CMake and compiles DSO into a separate build directory

---

## 4. Build (Manual)

If the automatic script breaks halfway, do it step by step:

```bash
# 1) Install apt dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake git pkg-config \
    libsuitesparse-dev libeigen3-dev \
    libboost-system-dev libboost-thread-dev \
    libopencv-dev \
    libgl1-mesa-dev libglew-dev libegl1-mesa-dev \
    libwayland-dev libxkbcommon-dev wayland-protocols \
    libavcodec-dev libavutil-dev libavformat-dev libswscale-dev libavdevice-dev

# 2) Initialize submodule (sse2neon)
git submodule update --init --recursive

# 3) Install Pangolin from source (one-time)
git clone --recursive --branch v0.9.1 https://github.com/stevenlovegrove/Pangolin.git ~/Pangolin
cd ~/Pangolin && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_PANGOLIN_PYTHON=OFF
make -j2 && sudo make install && sudo ldconfig

# 4) Build DSO (back in the project folder)
cd /path/to/DSO
mkdir -p build_ubuntu      # or build_arm on Jetson
cd build_ubuntu
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j2
```

---

## 5. Running

After a successful build, three binaries are available in `build_<arch>/bin/`:

| Binary        | Purpose                                                              |
| ------------- | -------------------------------------------------------------------- |
| `dso_dataset` | Plays back an offline dataset (image sequence + calibration file)    |
| `dso_live`    | Live capture from a video source (file/stream) via OpenCV            |
| `dso_camera`  | Real-time capture straight from a camera (USB / CSI)                 |

Example: running a dataset

```bash
cd build_ubuntu/bin        # or build_arm/bin
./dso_dataset files=/path/to/sequence calib=/path/to/camera.txt mode=1
```

A template calibration file lives at `camera.txt` in the project root.
Detailed mode/option descriptions are in the original [README](README.md).

### Pangolin GUI & Fisheye Shader

`dso_dataset`, `dso_live`, and `dso_camera` all open a Pangolin window with
sliders for the fisheye shader parameters (alpha, beta, zoom, etc.). Four
shader modes are available:

- `anypoint_mode1` — virtual perspective camera (alpha/beta + zoom)
- `anypoint_mode2` — same, but using Euler angles (thetaX/thetaY)
- `panorama_tube`  — 360° cylindrical unwarp
- `panorama_car`   — bird's-eye panorama for vehicle-mounted cameras

The shader files live in `shaders/` and are **loaded at runtime**, so you can
edit them without rebuilding — just restart the binary.

---

## 6. Troubleshooting

### `./build_*.sh: Permission denied` or `command not found`

The execute bit was lost after clone/extract. Fix:

```bash
chmod +x build_ubuntu.sh
./build_ubuntu.sh
```

Or invoke via bash directly (no chmod needed):

```bash
bash build_ubuntu.sh
```

### `set: Illegal option -o pipefail`

Already fixed in the latest version — the scripts now use `set -e` only so
they run under both dash (`sh`) and bash. If you still see this, make sure
you have the latest build scripts from this repo.

### `git: not found`

Already fixed in the latest version — `apt-get install` (which installs `git`
as part of the dependency set) now runs first, **before** `git submodule
update`. If it still appears, install manually:

```bash
sudo apt-get update && sudo apt-get install -y git
```

### `libboost-python1.74-dev : Depends: python3-dev:any`

A classic dependency conflict on fresh Ubuntu 22.04 when using
`libboost-all-dev`. Fixed in the scripts — we only install
`libboost-system-dev` and `libboost-thread-dev` (the Boost components DSO
actually uses). If you already installed `libboost-all-dev`, remove it first:

```bash
sudo apt-get remove --purge libboost-all-dev
sudo apt-get autoremove
./build_ubuntu.sh
```

### Build crash / OOM ("internal compiler error", "Killed")

Too many parallel compile jobs. Turn them down:

```bash
JOBS=2 ./build_ubuntu.sh
# or the most conservative:
JOBS=1 ./build_jetson.sh
```

### `Pangolin not found`

The script will build it from source automatically. To force a rebuild:

```bash
rm -rf ~/Pangolin
./build_ubuntu.sh
```

### `fatal error: SSE2NEON.h: No such file`

The `sse2neon` submodule wasn't cloned. Run:

```bash
git submodule update --init --recursive
```

### `undefined reference to cholmod_*` / `cxsparse_*`

`libsuitesparse-dev` isn't installed:

```bash
sudo apt-get install -y libsuitesparse-dev
```

### Wrong architecture

The script refuses to run on the wrong host:

- `build_jetson.sh` only runs on aarch64 (`uname -m` → `aarch64`)
- `build_ubuntu.sh` only runs on x86_64 (`uname -m` → `x86_64`)

### Clean rebuild

```bash
rm -rf build_arm build_ubuntu
./build_ubuntu.sh        # or build_jetson.sh
```

---

## 7. Directory Layout

```
DSO/
├── build_jetson.sh        # ARM/Jetson build script      → build_arm/
├── build_ubuntu.sh        # Ubuntu x86_64 build script   → build_ubuntu/
├── CMakeLists.txt         # auto-detects ARM vs x86_64
├── INSTALL.md             # this file
├── README.md              # project overview
├── camera.txt             # camera calibration template
├── cmake/                 # CMake modules (FindSuiteParse, etc.)
├── shaders/               # GLSL fisheye shaders (loaded at runtime)
├── src/
│   ├── FullSystem/        # core SLAM (tracking + mapping)
│   ├── OptimizationBackend/ # photometric bundle adjustment
│   ├── IOWrapper/         # Pangolin viewer + shader manager + OpenCV IO
│   └── util/              # calibration, undistort, AnypointRemap (CPU)
└── thirdparty/
    ├── Sophus/            # Lie group library (SE3/Sim3)
    └── sse2neon/          # SSE→NEON shim (only used on ARM builds)
```

---

## 8. For Maintainers — Before Pushing to GitHub

Make sure the executable bit on the build scripts is stored in the git index,
so anyone who clones can run `./build_*.sh` directly without chmod:

```bash
chmod +x build_jetson.sh build_ubuntu.sh
git update-index --chmod=+x build_jetson.sh build_ubuntu.sh
git add build_jetson.sh build_ubuntu.sh
git commit -m "Make build scripts executable"
git push
```

Verify with `git ls-files --stage build_*.sh` — the first column should be
`100755` (executable), not `100644`.

---

## 9. License

See [LICENSE](LICENSE). The original DSO is released under GPLv3 by its
original authors (J. Engel, V. Koltun, D. Cremers).
