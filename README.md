# DSO — Direct Sparse Odometry dengan Pangolin Fisheye Shader

Implementasi **Direct Sparse Odometry (DSO)** dengan integrasi **shader Pangolin**
untuk visualisasi live kamera fisheye. Mendukung build di **ARM (Jetson)** maupun
**Ubuntu x86_64 biasa**.

> Repository ini berbasis pada [DSO asli oleh Jakob Engel](https://github.com/JakobEngel/dso)
> (TU Munich, GPLv3), dengan tambahan modul GPU shader untuk unwarping fisheye
> dan dukungan multi-arsitektur. Lihat bagian [Lisensi](#lisensi) di bawah.

---

## Fitur Utama

- **Multi-arsitektur** — auto-detect ARM vs x86_64 di `CMakeLists.txt`
  - ARM: pakai `thirdparty/sse2neon` untuk translasi SSE → NEON
  - x86_64: pakai header SSE/AVX bawaan toolchain
- **Build script terpisah** yang tidak saling tabrakan:
  - `build_jetson.sh` → output ke `build_arm/`
  - `build_ubuntu.sh` → output ke `build_ubuntu/`
- **4 mode shader fisheye GPU** (dimuat saat runtime, bisa diedit tanpa rebuild):
  - `anypoint_mode1` — virtual perspective camera (alpha/beta + zoom)
  - `anypoint_mode2` — Euler angles (thetaX/thetaY)
  - `panorama_tube`  — unwarp 360° silindris
  - `panorama_car`   — bird's-eye panorama untuk kamera kendaraan
- **Dual-path processing** — GPU untuk visualisasi, CPU (`AnypointRemap`) untuk
  feature detection pipeline
- **Hardening untuk fresh install** — script otomatis install semua dependency,
  build Pangolin dari source kalau belum ada, dan batasi parallel job untuk
  menghindari OOM

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

Binary akan ada di `build_ubuntu/bin/` atau `build_arm/bin/`.

📖 **Panduan instalasi lengkap, troubleshooting, dan cara menjalankan ada di
[INSTALL.md](INSTALL.md).**

---

## Struktur Project

```
DSO/
├── build_jetson.sh        # Build script ARM/Jetson      → build_arm/
├── build_ubuntu.sh        # Build script Ubuntu x86_64   → build_ubuntu/
├── CMakeLists.txt         # Auto-detect ARM vs x86_64
├── INSTALL.md             # Panduan instalasi lengkap
├── README.md              # File ini
├── camera.txt             # Template file kalibrasi kamera
├── cmake/                 # Modul CMake (FindSuiteParse, dll.)
├── shaders/               # GLSL shader fisheye (dimuat runtime)
├── src/
│   ├── FullSystem/        # Core SLAM (tracking + mapping)
│   ├── OptimizationBackend/   # Photometric bundle adjustment
│   ├── IOWrapper/         # Pangolin viewer + shader manager + OpenCV IO
│   └── util/              # Kalibrasi, undistort, AnypointRemap (CPU)
└── thirdparty/
    ├── Sophus/            # Lie group (SE3/Sim3)
    └── sse2neon/          # Shim SSE→NEON (hanya dipakai saat build ARM)
```

---

## Binary yang Dihasilkan

| Binary        | Untuk apa                                                          |
| ------------- | ------------------------------------------------------------------ |
| `dso_dataset` | Memutar dataset offline (sequence gambar + file kalibrasi)         |
| `dso_live`    | Live capture dari source video (file/streaming) via OpenCV         |
| `dso_camera`  | Real-time capture langsung dari kamera (USB / CSI)                 |

Contoh menjalankan dataset:

```bash
cd build_ubuntu/bin
./dso_dataset files=/path/ke/sequence calib=/path/ke/camera.txt mode=1
```

Detail opsi runtime ada di [INSTALL.md](INSTALL.md) bagian "Menjalankan".

---

## Persyaratan Sistem

| Target           | Minimum                                          |
| ---------------- | ------------------------------------------------ |
| Jetson / ARM     | Jetson Orin Nano 8GB (atau board aarch64 setara) |
| Ubuntu x86_64    | CPU dengan SSE4.2, RAM ≥ 4GB (8GB direkomendasi) |
| OS               | Ubuntu 20.04 / 22.04 / 24.04                     |

Build script akan otomatis install semua dependency via `apt` (SuiteSparse,
Eigen3, Boost system+thread, OpenCV, OpenGL/GLEW, Wayland, FFmpeg) dan build
Pangolin v0.9.1 dari source kalau belum ada.

---

## Kontribusi & Issue

Buka issue di tab [Issues](https://github.com/Rafly-88/DSO/issues) kalau
menemukan bug atau ingin request fitur. Pull request welcome.

---

## Referensi

Paper DSO asli — bacalah ini untuk memahami algoritma dasarnya:

- J. Engel, V. Koltun, D. Cremers — *Direct Sparse Odometry*, arXiv:1607.02565, 2016
- J. Engel, V. Usenko, D. Cremers — *A Photometrically Calibrated Benchmark For
  Monocular Visual Odometry*, arXiv:1607.02555, 2016

Dataset publik untuk testing: <https://vision.in.tum.de/mono-dataset>

---

## Lisensi

Repository ini dirilis di bawah **GPLv3** (lihat [LICENSE](LICENSE)),
mengikuti lisensi DSO asli oleh Jakob Engel dan kolaborator (TU Munich).

Modifikasi dan tambahan di repository ini (Pangolin fisheye shader integration,
ARM/Ubuntu build scripts, dokumentasi) © Rafly-88, juga di bawah GPLv3.
