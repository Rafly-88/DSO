# DSO — Direct Sparse Odometry with Pangolin Fisheye Shader

A **Direct Sparse Odometry (DSO)** implementation with integrated **Pangolin
shader** support for live fisheye-camera visualization. Builds on both
**ARM (Jetson)** and **standard Ubuntu x86_64**.

> This repository is based on [the original DSO by Jakob Engel](https://github.com/JakobEngel/dso)
> (TU Munich, GPLv3), extended with a GPU shader module for fisheye unwarping
> and multi-architecture build support. See the [License](#license) section
> at the bottom.

---

## Key Features

- **Multi-architecture** — `CMakeLists.txt` auto-detects ARM vs x86_64
  - ARM: uses `thirdparty/sse2neon` to translate SSE → NEON
  - x86_64: uses the toolchain's native SSE/AVX headers
- **Separate build scripts** with no output collision:
  - `build_jetson.sh` → outputs to `build_arm/`
  - `build_ubuntu.sh` → outputs to `build_ubuntu/`
- **Four GPU fisheye shader modes** (loaded at runtime, editable without rebuild):
  - `anypoint_mode1` — virtual perspective camera (alpha/beta + zoom)
  - `anypoint_mode2` — Euler angles (thetaX/thetaY)
  - `panorama_tube`  — 360° cylindrical unwarp
  - `panorama_car`   — bird's-eye panorama for vehicle-mounted cameras
- **Dual-path processing** — GPU for visualization, CPU (`AnypointRemap`) for
  the feature-detection pipeline
- **Hardened for fresh installs** — scripts auto-install all dependencies,
  build Pangolin from source if missing, and cap parallel jobs to avoid OOM

---

## Quick Start

### Ubuntu x86_64

```bash
git clone --recursive https://github.com/Rafly-88/DSO.git
cd DSO
chmod +x build_ubuntu.sh
./build_ubuntu.sh
```

### Jetson / ARM (aarch64)

```bash
git clone --recursive https://github.com/Rafly-88/DSO.git
cd DSO
chmod +x build_jetson.sh
./build_jetson.sh
```

Binaries land in `build_ubuntu/bin/` or `build_arm/bin/`.

📖 **Full installation guide, troubleshooting, and runtime usage live in
[INSTALL.md](INSTALL.md).**

---

## Project Layout

```
DSO/
├── build_jetson.sh        # Build script for ARM/Jetson    → build_arm/
├── build_ubuntu.sh        # Build script for Ubuntu x86_64 → build_ubuntu/
├── CMakeLists.txt         # Auto-detects ARM vs x86_64
├── INSTALL.md             # Full installation guide
├── README.md              # This file
├── camera.txt             # Camera calibration file template
├── cmake/                 # CMake modules (FindSuiteParse, etc.)
├── shaders/               # GLSL fisheye shaders (loaded at runtime)
├── src/
│   ├── FullSystem/        # Core SLAM (tracking + mapping)
│   ├── OptimizationBackend/   # Photometric bundle adjustment
│   ├── IOWrapper/         # Pangolin viewer + shader manager + OpenCV IO
│   └── util/              # Calibration, undistort, AnypointRemap (CPU)
└── thirdparty/
    ├── Sophus/            # Lie group library (SE3/Sim3)
    └── sse2neon/          # SSE→NEON shim (used only on ARM builds)
```

---

## Produced Binaries

| Binary        | Purpose                                                              |
| ------------- | -------------------------------------------------------------------- |
| `dso_dataset` | Plays back an offline dataset (image sequence + calibration file)    |
| `dso_live`    | Live capture from a video source (file/stream) via OpenCV            |
| `dso_camera`  | Real-time capture straight from a camera (USB / CSI)                 |

Example: running on a dataset

```bash
cd build_ubuntu/bin
./dso_dataset files=/path/to/sequence calib=/path/to/camera.txt mode=1
```

Full runtime options are documented in the "Running" section of
[INSTALL.md](INSTALL.md).

---

## System Requirements

| Target           | Minimum                                            |
| ---------------- | -------------------------------------------------- |
| Jetson / ARM     | Jetson Orin Nano 8GB (or equivalent aarch64 board) |
| Ubuntu x86_64    | CPU with SSE4.2, ≥ 4GB RAM (8GB recommended)       |
| OS               | Ubuntu 20.04 / 22.04 / 24.04                       |

The build scripts auto-install all dependencies via `apt` (SuiteSparse,
Eigen3, Boost system+thread, OpenCV, OpenGL/GLEW, Wayland, FFmpeg) and build
Pangolin v0.9.1 from source if it's not already present.

---

## Contributing & Issues

Open an issue under [Issues](https://github.com/Rafly-88/DSO/issues) for bugs
or feature requests. Pull requests are welcome.

---

## References

Original DSO papers — read these to understand the underlying algorithm:

- J. Engel, V. Koltun, D. Cremers — *Direct Sparse Odometry*, arXiv:1607.02565, 2016
- J. Engel, V. Usenko, D. Cremers — *A Photometrically Calibrated Benchmark For
  Monocular Visual Odometry*, arXiv:1607.02555, 2016

Public dataset for testing: <https://vision.in.tum.de/mono-dataset>

---

## License

This repository is released under **GPLv3** (see [LICENSE](LICENSE)),
following the license of the original DSO by Jakob Engel and collaborators
(TU Munich).

Modifications and additions in this repository (Pangolin fisheye shader
integration, ARM/Ubuntu build scripts, documentation) © Rafly-88, also under
GPLv3.
