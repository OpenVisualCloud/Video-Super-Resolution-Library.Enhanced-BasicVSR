# Manual Build Steps for FFmpeg + IVSR plugin Software on Ubuntu

This document provides detailed steps for building the software with FFmpeg + iVSR SDK as the backend to work for media transcoding and DNN-based processing for video content on a clean Ubuntu 22.04 system.

## Prerequisites

Ensure your system has internet access and an updated package index:

```bash
sudo apt-get update
```

## Step-by-Step Instructions

### 1. Install Essential Utilities

Start by installing essential packages required for downloading and handling other software components:

```bash
sudo apt-get install -y --no-install-recommends \
    curl ca-certificates gpg-agent software-properties-common
```
Install common dependencies:
```bash
sudo apt-get install -y --no-install-recommends --fix-missing \
    autoconf \
    automake \
    build-essential \
    apt-utils cmake cython3 flex bison gcc g++ git make patch pkg-config wget \
    libdrm-dev libudev-dev libtool libusb-1.0-0-dev xz-utils ocl-icd-opencl-dev opencl-headers
```
### 2. Set Up OpenVINO

Set up the OpenVINO toolkit by downloading and installing the key, adding the repository, and installing OpenVINO:

```bash
wget https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB
sudo apt-key add GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB

echo "deb https://apt.repos.intel.com/openvino/2024 ubuntu22 main" | sudo tee /etc/apt/sources.list.d/intel-openvino-2024.list

sudo apt-get update
sudo apt-get install -y openvino-2024.5.0
rm -f GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB
```

### 3. Install FFmpeg Dependencies

Install additional dependencies required by FFmpeg:

```bash
sudo apt-get install -y --no-install-recommends \
    ca-certificates tar g++ wget pkg-config nasm yasm libglib2.0-dev flex bison gobject-introspection libgirepository1.0-dev \
    python3-dev libx11-dev libxv-dev libxt-dev libasound2-dev libpango1.0-dev libtheora-dev libvisual-0.4-dev libgl1-mesa-dev \
    libcurl4-gnutls-dev librtmp-dev mjpegtools libx264-dev libx265-dev libde265-dev libva-dev libtbb-dev
```

### 4. Build iVSR SDK

1. Clone or copy the iVSR SDK repository into your workspace.
2. Navigate to the SDK folder and create a build directory.
3. Run CMake with the appropriate flags and build the project.

```bash
mkdir -p <workspace>/ivsr/ivsr_sdk/build
cd <workspace>/ivsr/ivsr_sdk/build
cmake .. -DENABLE_LOG=OFF -DENABLE_PERF=OFF -DENABLE_THREADPROCESS=ON -DCMAKE_BUILD_TYPE=Release
make -j $(nproc --all)
sudo make install
```

### 5. Build and Install FFmpeg with iVSR SDK Support

1. Configure global Git settings if you didn't.
2. Clone the FFmpeg repository and check out the desired version.
3. Apply necessary patches and configure the build.
4. Compile and install FFmpeg.

```bash
git config --global user.email "noname@example.com"
git config --global user.name "no name"
git clone https://github.com/FFmpeg/FFmpeg.git <workspace>/ivsr/ivsr_ffmpeg_plugin/ffmpeg
cd <workspace>/ivsr/ivsr_ffmpeg_plugin/ffmpeg
git checkout n6.1

# Apply patches
cp -rf <workspace>/ivsr/ivsr_ffmpeg_plugin/patches/*.patch .
git am --whitespace=fix *.patch

./configure \
    --enable-gpl \
    --enable-nonfree \
    --disable-static \
    --disable-doc \
    --enable-shared \
    --enable-version3 \
    --enable-libivsr \
    --enable-libx264 \
    --enable-libx265

make -j$(nproc)
sudo make install

# Set the library path for FFmpeg to ensure it can find the necessary shared libraries
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Run ffmpeg to test if it can run successfully
ffmpeg
```

### 6. Install GPU Drivers (Optional)

Install required GPU drivers and dependencies:

```bash
sudo apt-get install -y --no-install-recommends ocl-icd-libopencl1

# Download and install necessary GPU packages
mkdir /tmp/gpu_deps && cd /tmp/gpu_deps
curl -L -O https://github.com/intel/intel-graphics-compiler/releases/download/igc-1.0.17384.11/intel-igc-core_1.0.17384.11_amd64.deb
curl -L -O https://github.com/intel/intel-graphics-compiler/releases/download/igc-1.0.17384.11/intel-igc-opencl_1.0.17384.11_amd64.deb
curl -L -O https://github.com/intel/compute-runtime/releases/download/24.31.30508.7/intel-level-zero-gpu-dbgsym_1.3.30508.7_amd64.ddeb
curl -L -O https://github.com/intel/compute-runtime/releases/download/24.31.30508.7/intel-level-zero-gpu_1.3.30508.7_amd64.deb
curl -L -O https://github.com/intel/compute-runtime/releases/download/24.31.30508.7/intel-opencl-icd-dbgsym_24.31.30508.7_amd64.ddeb
curl -L -O https://github.com/intel/compute-runtime/releases/download/24.31.30508.7/intel-opencl-icd_24.31.30508.7_amd64.deb
curl -L -O https://github.com/intel/compute-runtime/releases/download/24.31.30508.7/libigdgmm12_22.4.1_amd64.deb
sudo dpkg -i ./*.deb
rm -Rf /tmp/gpu_deps
```
Also, you can download the latest gpu driver from the [official website](https://github.com/intel/compute-runtime/releases).

### 7. Environment Configuration for GPU

Set the environment variables required for GPU drivers:

```bash
# Set the driver name for VA-API to use Intel's iHD driver
export LIBVA_DRIVER_NAME=iHD

# Set the path where VA-API can find the driver
export LIBVA_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri
```

Congratulations! You've successfully built the software on a bare metal Ubuntu system.

### Optional: Build and Install OpenCV

Start building OpenCV:

```bash
OPENCV_REPO=https://github.com/opencv/opencv/archive/4.5.3-openvino-2021.4.2.tar.gz
wget -qO - ${OPENCV_REPO} | tar xz
OPENCV_BASE=opencv-4.5.3-openvino-2021.4.2
cd ${OPENCV_BASE} && mkdir -p build && mkdir -p install && cd build
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=${OPENCV_BASE}/install \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DOPENCV_GENERATE_PKGCONFIG=ON \
    -DBUILD_DOCS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_PERF_TESTS=OFF \
    -DBUILD_TESTS=OFF \
    -DWITH_OPENEXR=OFF \
    -DWITH_OPENJPEG=OFF \
    -DWITH_GSTREAMER=OFF \
    -DWITH_JASPER=OFF \
    -DWITH_FFMPEG=OFF \
    -DPYTHON3_EXECUTABLE=/usr/bin/python3 \
    ..

make -j "$(nproc)"
sudo make install
# The setup_vars_opencv4.sh script sets up the environment variables required for OpenCV.
cd ${OPENCV_BASE}/install/bin && bash ./setup_vars_opencv4.sh
```
