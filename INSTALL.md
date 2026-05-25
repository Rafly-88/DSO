# Panduan Instalasi DSO (Direct Sparse Odometry)

Implementasi DSO dengan integrasi shader Pangolin untuk visualisasi fisheye.
Mendukung dua target build yang **saling terpisah** (artefak ditaruh di
direktori berbeda, tidak akan saling timpa):

| Target           | Build script        | Output directory  |
| ---------------- | ------------------- | ----------------- |
| ARM / Jetson     | `build_jetson.sh`   | `build_arm/`      |
| Ubuntu x86_64    | `build_ubuntu.sh`   | `build_ubuntu/`   |

`CMakeLists.txt` otomatis mendeteksi arsitektur (`CMAKE_SYSTEM_PROCESSOR`)
dan mengaktifkan flag yang tepat:

- **aarch64 / arm** → include `thirdparty/sse2neon` (shim SSE → NEON)
- **x86_64** → pakai header SSE/AVX bawaan toolchain + `-msse4.2`

---

## Quick Start

Untuk yang ingin langsung jalan di Ubuntu x86_64:

```bash
git clone --recursive https://github.com/Rafly-88/DSO.git
cd <REPO>
chmod +x build_ubuntu.sh
./build_ubuntu.sh
```

Setelah selesai, binary ada di `build_ubuntu/bin/`. Sisanya di bawah.

---

## 1. Prasyarat

### Hardware

| Target           | Minimum                                          |
| ---------------- | ------------------------------------------------ |
| Jetson / ARM     | Jetson Orin Nano 8GB (atau board aarch64 setara) |
| Ubuntu x86_64    | CPU dengan SSE4.2, RAM ≥ 4GB (8GB direkomendasi) |

### Sistem Operasi

- Ubuntu 20.04 / 22.04 / 24.04 (x86_64)
- Ubuntu 22.04 di Jetson (JetPack 6.x)

### Dependencies utama

Build script akan otomatis menginstal semuanya via `apt`:

- `build-essential`, `cmake`, `git`, `pkg-config`
- `libsuitesparse-dev`, `libeigen3-dev`
- `libboost-system-dev`, `libboost-thread-dev`
  (DSO hanya butuh dua komponen Boost ini — **jangan** pakai `libboost-all-dev`
  yang menarik `libboost-python1.74-dev` dan bentrok dengan `python3-dev`
  di Ubuntu fresh install)
- `libopencv-dev`
- `libgl1-mesa-dev`, `libglew-dev`, `libegl1-mesa-dev`
- `libwayland-dev`, `libxkbcommon-dev`, `wayland-protocols`
- `libavcodec-dev`, `libavutil-dev`, `libavformat-dev`, `libswscale-dev`, `libavdevice-dev`

**Pangolin** (v0.9.1) akan otomatis di-clone & di-build dari source kalau
belum terdeteksi di sistem (di-install ke `/usr/local`).

---

## 2. Clone Repository

Repository ini menggunakan git submodule (`thirdparty/sse2neon`), jadi
gunakan `--recursive`:

```bash
git clone --recursive https://github.com/Rafly-88/DSO.git
cd <REPO>
```

Kalau sudah terlanjur clone tanpa `--recursive`:

```bash
git submodule update --init --recursive
```

> **Catatan**: kalau Anda tidak punya `git` di sistem fresh, jangan khawatir —
> build script akan install `git` lewat apt terlebih dahulu, baru kemudian
> menjalankan `git submodule update`. Jadi cukup ekstrak ZIP dari GitHub
> lalu jalankan build script-nya.

---

## 3. Build (Otomatis — disarankan)

### Di Ubuntu x86_64

```bash
chmod +x build_ubuntu.sh
./build_ubuntu.sh
```

Output → `build_ubuntu/bin/` dan `build_ubuntu/lib/libdso.a`

### Di Jetson / ARM (aarch64)

```bash
chmod +x build_jetson.sh
./build_jetson.sh
```

Output → `build_arm/bin/` dan `build_arm/lib/libdso.a`

### Cara menjalankan kalau `./build_*.sh` gagal

Ada tiga cara setara untuk menjalankan script:

```bash
chmod +x build_ubuntu.sh && ./build_ubuntu.sh   # standar
bash build_ubuntu.sh                            # tanpa perlu chmod
sh   build_ubuntu.sh                            # juga jalan
```

### ⚠️ JANGAN jalankan dengan `sudo`

```bash
# ❌ JANGAN
sudo ./build_ubuntu.sh

# ✅ BENAR
./build_ubuntu.sh
```

Script ini sudah memanggil `sudo apt-get install` di dalam saat dibutuhkan.
Kalau seluruh script dijalankan dengan `sudo`:

- `$HOME` jadi `/root` → Pangolin akan ter-clone ke `/root/Pangolin`
- Folder `build_ubuntu/` jadi milik root → susah dihapus tanpa sudo

Jalankan sebagai user biasa; nanti sudo akan minta password sekali untuk `apt`.

### Mengatur jumlah thread compile

Secara default kedua script pakai **setengah** dari core CPU agar tidak OOM
(template Eigen/Sophus sangat berat di memori saat compile). Override dengan
env var `JOBS=N`:

```bash
JOBS=4 ./build_ubuntu.sh   # pakai 4 thread
JOBS=2 ./build_jetson.sh   # paling aman kalau RAM tipis
JOBS=1 ./build_jetson.sh   # serial — paling lambat tapi paling stabil
```

### Apa saja yang dikerjakan script

1. Cek arsitektur host (gagal cepat kalau salah pakai)
2. `apt-get update` dan install **semua** dependency (termasuk `git`, `cmake`)
3. `git submodule update --init --recursive` untuk `thirdparty/sse2neon`
4. Build & install Pangolin v0.9.1 dari source kalau belum ada
5. Konfigurasi CMake & compile DSO ke direktori build terpisah

---

## 4. Build (Manual)

Kalau script otomatis gagal di tengah jalan, ikuti langkah-langkah ini:

```bash
# 1) Install dependency apt
sudo apt-get update
sudo apt-get install -y build-essential cmake git pkg-config \
    libsuitesparse-dev libeigen3-dev \
    libboost-system-dev libboost-thread-dev \
    libopencv-dev \
    libgl1-mesa-dev libglew-dev libegl1-mesa-dev \
    libwayland-dev libxkbcommon-dev wayland-protocols \
    libavcodec-dev libavutil-dev libavformat-dev libswscale-dev libavdevice-dev

# 2) Init submodule (sse2neon)
git submodule update --init --recursive

# 3) Install Pangolin dari source (sekali saja)
git clone --recursive --branch v0.9.1 https://github.com/stevenlovegrove/Pangolin.git ~/Pangolin
cd ~/Pangolin && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_PANGOLIN_PYTHON=OFF
make -j2 && sudo make install && sudo ldconfig

# 4) Build DSO (kembali ke folder project)
cd /path/ke/dsoorin
mkdir -p build_ubuntu      # atau build_arm di Jetson
cd build_ubuntu
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j2
```

---

## 5. Menjalankan

Setelah build sukses, tiga binary tersedia di `build_<arch>/bin/`:

| Binary        | Untuk apa                                                          |
| ------------- | ------------------------------------------------------------------ |
| `dso_dataset` | Memutar dataset offline (sequence gambar + file kalibrasi)         |
| `dso_live`    | Live capture dari source video (file/streaming) via OpenCV         |
| `dso_camera`  | Real-time capture langsung dari kamera (USB / CSI)                 |

Contoh menjalankan dataset:

```bash
cd build_ubuntu/bin        # atau build_arm/bin
./dso_dataset files=/path/ke/sequence calib=/path/ke/camera.txt mode=1
```

Format file kalibrasi ada di `camera.txt` di root project (template).
Penjelasan tiap mode & opsi ada di [README.md](README.md).

### GUI Pangolin & Shader Fisheye

`dso_dataset`, `dso_live`, dan `dso_camera` akan membuka window Pangolin
dengan slider untuk parameter shader fisheye (alpha, beta, zoom, dst).
Empat mode shader tersedia:

- `anypoint_mode1` — virtual perspective camera (rotasi alpha/beta + zoom)
- `anypoint_mode2` — sama, tapi pakai Euler angles (thetaX/thetaY)
- `panorama_tube`  — unwarp 360° silindris
- `panorama_car`   — bird's-eye panorama untuk kamera kendaraan

File shader ada di `shaders/` dan **dimuat saat runtime**, jadi bisa diedit
tanpa rebuild — tinggal restart binary-nya.

---

## 6. Troubleshooting

### `./build_*.sh: Permission denied` atau `command not found`

Execute bit hilang setelah clone/extract. Solusi:

```bash
chmod +x build_ubuntu.sh
./build_ubuntu.sh
```

atau panggil langsung dengan bash (tanpa perlu chmod):

```bash
bash build_ubuntu.sh
```

### `set: Illegal option -o pipefail`

Sudah diperbaiki di versi terbaru — script sekarang pakai `set -e` saja
supaya jalan di dash (`sh`) maupun bash. Kalau masih muncul, pastikan
Anda pakai file build script versi terbaru dari repo ini.

### `git: not found`

Sudah diperbaiki di versi terbaru — `apt-get install` (yang ikut install `git`)
sekarang dijalankan paling awal, **sebelum** `git submodule update`. Kalau
masih muncul, install manual:

```bash
sudo apt-get update && sudo apt-get install -y git
```

### `libboost-python1.74-dev : Depends: python3-dev:any`

Konflik klasik di Ubuntu 22.04 fresh saat memakai `libboost-all-dev`.
Sudah diperbaiki di script — kita hanya install `libboost-system-dev` dan
`libboost-thread-dev` (komponen Boost yang sebenarnya dipakai DSO).
Kalau Anda terlanjur install `libboost-all-dev`, hapus dulu:

```bash
sudo apt-get remove --purge libboost-all-dev
sudo apt-get autoremove
./build_ubuntu.sh
```

### Build crash / OOM ("internal compiler error", "Killed")

Terlalu banyak thread paralel saat compile. Turunkan:

```bash
JOBS=2 ./build_ubuntu.sh
# atau paling konservatif:
JOBS=1 ./build_jetson.sh
```

### `Pangolin not found`

Script otomatis akan build dari source. Kalau ingin force rebuild:

```bash
rm -rf ~/Pangolin
./build_ubuntu.sh
```

### `fatal error: SSE2NEON.h: No such file`

Submodule `sse2neon` belum di-clone. Jalankan:

```bash
git submodule update --init --recursive
```

### `undefined reference to cholmod_*` / `cxsparse_*`

`libsuitesparse-dev` belum terpasang:

```bash
sudo apt-get install -y libsuitesparse-dev
```

### Salah arsitektur

Script akan menolak kalau dijalankan di host yang salah:

- `build_jetson.sh` hanya jalan di aarch64 (`uname -m` → `aarch64`)
- `build_ubuntu.sh` hanya jalan di x86_64 (`uname -m` → `x86_64`)

### Rebuild bersih

```bash
rm -rf build_arm build_ubuntu
./build_ubuntu.sh        # atau build_jetson.sh
```

---

## 7. Struktur Direktori

```
dsoorin/
├── build_jetson.sh        # build script ARM/Jetson      → build_arm/
├── build_ubuntu.sh        # build script Ubuntu x86_64   → build_ubuntu/
├── CMakeLists.txt         # auto-detect ARM vs x86_64
├── INSTALL.md             # file ini
├── README.md              # dokumentasi DSO asli
├── camera.txt             # template file kalibrasi kamera
├── cmake/                 # modul CMake (FindSuiteParse, dll.)
├── shaders/               # GLSL shader fisheye (dimuat runtime)
├── src/
│   ├── FullSystem/        # core SLAM (tracking + mapping)
│   ├── OptimizationBackend/ # photometric bundle adjustment
│   ├── IOWrapper/         # Pangolin viewer + shader manager + OpenCV IO
│   └── util/              # kalibrasi, undistort, AnypointRemap (CPU)
└── thirdparty/
    ├── Sophus/            # Lie group (SE3/Sim3)
    └── sse2neon/          # shim SSE→NEON (hanya dipakai saat build ARM)
```

---

## 8. Untuk Maintainer — Sebelum Push ke GitHub

Pastikan executable bit dari build script tersimpan di git index, supaya
orang lain yang clone langsung bisa `./build_*.sh` tanpa perlu chmod dulu:

```bash
chmod +x build_jetson.sh build_ubuntu.sh
git update-index --chmod=+x build_jetson.sh build_ubuntu.sh
git add build_jetson.sh build_ubuntu.sh
git commit -m "Make build scripts executable"
git push
```

Cek dengan `git ls-files --stage build_*.sh` — bit pertama harus `100755`
(executable), bukan `100644`.

---

## 9. Lisensi

Lihat [LICENSE](LICENSE). DSO asli rilis di bawah GPLv3 oleh penulis aslinya
(J. Engel, V. Koltun, D. Cremers).
