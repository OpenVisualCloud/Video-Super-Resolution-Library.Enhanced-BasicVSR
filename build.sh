#!/bin/bash

set -e

base_dir=$(pwd)
ov_version=2022.3

# Function to display usage information
usage() {
    echo "Usage: $0  --ov_version [2022.3|2023.2|2024.5]"
    exit 1
}

prepare_dependencies() {
  echo "Preparing dependencies..."
  sudo apt-get install -y --no-install-recommends \
    curl ca-certificates gpg-agent software-properties-common
  
  sudo apt-get install -y --no-install-recommends --fix-missing \
    autoconf \
    automake \
    build-essential \
    apt-utils cmake cython3 flex bison gcc g++ git make patch pkg-config wget \
    libdrm-dev libudev-dev libtool libusb-1.0-0-dev xz-utils ocl-icd-opencl-dev opencl-headers \
    apt-utils gpg-agent software-properties-common wget python3-dev libpython3-dev python3-pip
}

install_openvino_from_source() {
  if [ -z "$(git config --global user.name)" ]; then
      git config --global user.name "no name"
  fi

  if [ -z "$(git config --global user.email)" ]; then
      git config --global user.email "noname@example.com"
  fi

  echo "Start building OpenVINO"

  ov_repo=https://github.com/openvinotoolkit/openvino.git
  ov_branch=${ov_version}.0
  ivsr_ov_dir=${base_dir}/ivsr_ov/based_on_openvino_${ov_version}/openvino
  if [ ! -d "${ivsr_ov_dir}" ]; then
    git clone --depth 1 --branch ${ov_branch} ${ov_repo} ${ivsr_ov_dir}
    git config --global --add safe.directory ${ivsr_ov_dir}
  fi
  cd ${ivsr_ov_dir}
  #git checkout ${OV_BRANCH}
  git submodule update --init --recursive

  ## applying ov22.3 patches to enable Enhanced BasicVSR model
  for patch_file in $(find ../patches -iname "*.patch" | sort -n);do
      echo "Applying: ${patch_file}"
      git am --whitespace=fix ${patch_file}
  done

  mkdir -p build && cd build && \
  cmake \
    -DCMAKE_INSTALL_PREFIX=${PWD}/../install \
    -DENABLE_INTEL_CPU=ON \
    -DENABLE_CLDNN=ON \
    -DENABLE_INTEL_GPU=ON \
    -DENABLE_ONEDNN_FOR_GPU=OFF \
    -DENABLE_INTEL_GNA=OFF \
    -DENABLE_INTEL_MYRIAD_COMMON=OFF \
    -DENABLE_INTEL_MYRIAD=OFF \
    -DENABLE_PYTHON=ON \
    -DENABLE_OPENCV=ON \
    -DENABLE_SAMPLES=ON \
    -DENABLE_CPPLINT=OFF \
    -DTREAT_WARNING_AS_ERROR=OFF \
    -DENABLE_TESTS=OFF \
    -DENABLE_GAPI_TESTS=OFF \
    -DENABLE_BEH_TESTS=OFF \
    -DENABLE_FUNCTIONAL_TESTS=OFF \
    -DENABLE_OV_CORE_UNIT_TESTS=OFF \
    -DENABLE_OV_CORE_BACKEND_UNIT_TESTS=OFF \
    -DENABLE_DEBUG_CAPS=ON \
    -DENABLE_GPU_DEBUG_CAPS=ON \
    -DENABLE_CPU_DEBUG_CAPS=ON \
    -DCMAKE_BUILD_TYPE=Release \
    .. && \
  make -j $(nproc --all) && \
  make install
  echo "Build OpenVINO finished."
    
  source ${PWD}/../install/setupvars.sh
}

build_install_ivsr_sdk() {
  echo "Building and installing iVSR SDK..."

  ivsr_sdk_dir=${base_dir}/ivsr_sdk/
  cd ${ivsr_sdk_dir}
  mkdir -p build && cd build && cmake \
    -DENABLE_LOG=OFF -DENABLE_PERF=OFF -DENABLE_THREADPROCESS=ON \
    -DCMAKE_BUILD_TYPE=Release .. && \
  make -j $(nproc --all)
  sudo make install
  echo "Build ivsr sdk finished."
}

build_ffmpeg() {
  echo "Building FFMPEG with specific libraries support..."
  sudo apt-get update && \
    DEBIAN_FRONTEND=noninteractive sudo apt-get install -y --no-install-recommends \
    ca-certificates tar g++ wget pkg-config nasm yasm libglib2.0-dev flex bison gobject-introspection libgirepository1.0-dev \
    python3-dev libx11-dev libxv-dev libxt-dev libasound2-dev libpango1.0-dev libtheora-dev libvisual-0.4-dev libgl1-mesa-dev \
    libcurl4-gnutls-dev librtmp-dev mjpegtools libx264-dev libx265-dev libde265-dev libva-dev libtbb-dev
  
  # Add commands to build FFMPEG from source
  ffmpeg_dir=$base_dir/ivsr_ffmpeg_plugin/ffmpeg
  ffmpeg_repo=https://github.com/FFmpeg/FFmpeg.git

  if [ ! -d "${ffmpeg_dir}" ]; then
    git clone --depth 1 --branch n6.1 ${ffmpeg_repo} ${ffmpeg_dir}
    git config --global --add safe.directory ${ffmpeg_dir}
  fi

  # Apply patches
  cd ${ffmpeg_dir} && cp -rf $base_dir/ivsr_ffmpeg_plugin/patches/*.patch .
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
  sudo ldconfig
}

install_openvino_from_apt() {
  echo "Installing OpenVINO from apt..."
  local version=$1

  wget -qO - https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | sudo apt-key add -

  echo "deb https://apt.repos.intel.com/openvino/2023 ubuntu22 main" | sudo tee /etc/apt/sources.list.d/intel-openvino-2023.list
  echo "deb https://apt.repos.intel.com/openvino/2024 ubuntu22 main" | sudo tee /etc/apt/sources.list.d/intel-openvino-2024.list

  sudo -E apt-get update && \
    DEBIAN_FRONTEND=noninteractive sudo -E apt-get install -y openvino-$version.0
}

main() {
  while [ "$1" != "" ]; do
    case $1 in
      --ov_version ) shift
                     ov_version=$1
                     ;;
      * ) usage
          exit 1
    esac
    shift
  done

  prepare_dependencies
  if [ "$ov_version" = "2022.3" ]; then
    install_openvino_from_source
  else
    install_openvino_from_apt "$ov_version" 
  fi
  build_install_ivsr_sdk
  build_ffmpeg
}

main "$@"