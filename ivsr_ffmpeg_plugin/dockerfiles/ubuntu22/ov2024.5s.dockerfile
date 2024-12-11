# SPDX-License-Identifier: BSD 3-Clause License
#
# Copyright (c) 2023, Intel Corporation
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# * Neither the name of the copyright holder nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Base image
ARG IMAGE=ubuntu:22.04
FROM $IMAGE AS base

# Set non-interactive frontend for package installation
ARG DEBIAN_FRONTEND=noninteractive

# Install essential utilities
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    curl ca-certificates gpg-agent software-properties-common && \
    rm -rf /var/lib/apt/lists/*

# Configure shell
SHELL ["/bin/bash", "-o", "pipefail", "-c"]

FROM base AS build
LABEL vendor="Intel Corporation"

# Create workspace directory
RUN mkdir -p /workspace/ivsr

ARG WORKSPACE=/workspace

# Install common dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends --fix-missing \
    autoconf \
    automake \
    build-essential \
    apt-utils cmake cython3 flex bison gcc g++ git make patch pkg-config wget \
    libdrm-dev libudev-dev libtool libusb-1.0-0-dev xz-utils ocl-icd-opencl-dev opencl-headers \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Install OpenVINO
RUN wget https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB && \
    apt-key add GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB && \
    echo "deb https://apt.repos.intel.com/openvino/2024 ubuntu22 main" | tee /etc/apt/sources.list.d/intel-openvino-2024.list && \
    apt-get update && \
    apt-cache search openvino && \
    apt-get install -y openvino-2024.5.0 && \
    rm -f GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB

# Set the working directory
WORKDIR /workspace

# Install FFmpeg dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    ca-certificates tar g++ wget pkg-config nasm yasm libglib2.0-dev flex bison gobject-introspection libgirepository1.0-dev \
    python3-dev libx11-dev libxv-dev libxt-dev libasound2-dev libpango1.0-dev libtheora-dev libvisual-0.4-dev libgl1-mesa-dev \
    libcurl4-gnutls-dev librtmp-dev mjpegtools libx264-dev libx265-dev libde265-dev libva-dev libtbb-dev && \
    rm -rf /var/lib/apt/lists/*

# Build iVSR SDK
COPY ./ivsr_sdk ${WORKSPACE}/ivsr/ivsr_sdk
WORKDIR ${WORKSPACE}/ivsr/ivsr_sdk
RUN mkdir -p build && cd build && \
    cmake .. -DENABLE_LOG=OFF -DENABLE_PERF=OFF -DENABLE_THREADPROCESS=ON -DCMAKE_BUILD_TYPE=Release && \
    make -j16 && \
    make install && \
    echo "Building iVSR SDK finished."

# Build and install FFmpeg with libopenvino support
# FFmpeg setup and build
ARG FFMPEG_REPO=https://github.com/FFmpeg/FFmpeg.git
ARG FFMPEG_VERSION=n6.1
ARG FFMPEG_IVSR_SDK_PLUGIN_DIR=${WORKSPACE}/ivsr/ivsr_ffmpeg_plugin
WORKDIR ${FFMPEG_IVSR_SDK_PLUGIN_DIR}/ffmpeg
RUN git config --global user.email "noname@example.com" && \
    git config --global user.name "no name" && \
    git clone ${FFMPEG_REPO} . && \
    git checkout ${FFMPEG_VERSION}

COPY ./ivsr_ffmpeg_plugin/patches/*.patch ./
RUN for patch_file in $(find -iname "*.patch" | sort -n); do \
    echo "Applying: ${patch_file}"; \
    git am --whitespace=fix ${patch_file}; \
    done

RUN ./configure \
    --enable-gpl \
    --enable-nonfree \
    --disable-static \
    --disable-doc \
    --enable-shared \
    --enable-version3 \
    --enable-libivsr \
    --enable-libx264 \
    --enable-libx265 && \
    make -j16 && \
    make install

# for GPU
RUN apt-get update && \
    apt-get install -y --no-install-recommends ocl-icd-libopencl1 && \
    apt-get clean ; \
    rm -rf /var/lib/apt/lists/* && rm -rf /tmp/*
# hadolint ignore=DL3003
# 24.31.30508.7
RUN mkdir /tmp/gpu_deps && cd /tmp/gpu_deps && \
    curl -L -O https://github.com/intel/intel-graphics-compiler/releases/download/igc-1.0.17384.11/intel-igc-core_1.0.17384.11_amd64.deb && \
    curl -L -O https://github.com/intel/intel-graphics-compiler/releases/download/igc-1.0.17384.11/intel-igc-opencl_1.0.17384.11_amd64.deb && \
    curl -L -O https://github.com/intel/compute-runtime/releases/download/24.31.30508.7/intel-level-zero-gpu-dbgsym_1.3.30508.7_amd64.ddeb && \
    curl -L -O https://github.com/intel/compute-runtime/releases/download/24.31.30508.7/intel-level-zero-gpu_1.3.30508.7_amd64.deb && \
    curl -L -O https://github.com/intel/compute-runtime/releases/download/24.31.30508.7/intel-opencl-icd-dbgsym_24.31.30508.7_amd64.ddeb && \
    curl -L -O https://github.com/intel/compute-runtime/releases/download/24.31.30508.7/intel-opencl-icd_24.31.30508.7_amd64.deb && \
    curl -L -O https://github.com/intel/compute-runtime/releases/download/24.31.30508.7/libigdgmm12_22.4.1_amd64.deb && \
    dpkg -i ./*.deb && rm -Rf /tmp/gpu_deps

ENV LIBVA_DRIVER_NAME=iHD
ENV LIBVA_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri

# Set the working directory back to /workspace
WORKDIR /workspace
CMD ["/bin/bash"]
