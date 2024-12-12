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
ARG IMAGE=ubuntu@sha256:0e5e4a57c2499249aafc3b40fcd541e9a456aab7296681a3994d631587203f97
FROM $IMAGE AS base

# Set non-interactive frontend for package installation
ARG DEBIAN_FRONTEND=noninteractive

# Install essential utilities
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    curl wget ca-certificates gpg-agent software-properties-common && \
    rm -rf /var/lib/apt/lists/*

# Configure shell
SHELL ["/bin/bash", "-o", "pipefail", "-c"]

FROM base AS build
LABEL vendor="Intel Corporation"

# Create workspace directory
RUN mkdir -p /workspace/ivsr

# Define build arguments
ARG ENABLE_OV_PATCH
ARG OV_VERSION
ARG WORKSPACE=/workspace
ARG PYTHON

# Install common dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends --fix-missing \
    apt-utils cmake cython3 flex bison gcc g++ git make patch pkg-config wget \
    libdrm-dev libudev-dev libtool libusb-1.0-0-dev xz-utils ocl-icd-opencl-dev opencl-headers \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Install Python and pip
RUN apt-get update && apt-get install -y --no-install-recommends --fix-missing \
    ${PYTHON} lib${PYTHON}-dev python3-pip && \
    apt-get clean && rm -rf /var/lib/apt/lists/* && \
    pip --no-cache-dir install --upgrade pip setuptools && \
    ln -sf $(which ${PYTHON}) /usr/local/bin/python && \
    ln -sf $(which ${PYTHON}) /usr/local/bin/python3 && \
    ln -sf $(which ${PYTHON}) /usr/bin/python && \
    ln -sf $(which ${PYTHON}) /usr/bin/python3

# Install and build OpenCV with OpenVINO
ARG OPENCV_REPO=https://github.com/opencv/opencv/archive/4.5.3-openvino-2021.4.2.tar.gz
WORKDIR ${WORKSPACE}
RUN wget -qO - ${OPENCV_REPO} | tar xz

WORKDIR ${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/
RUN mkdir build && mkdir install
WORKDIR ${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/build
RUN cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/install \
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
    .. && \
    make -j16 && \
    make install && \
    cd ${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/install/bin && bash ./setup_vars_opencv4.sh

ENV LD_LIBRARY_PATH=${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/install/lib:$LD_LIBRARY_PATH
ENV OpenCV_DIR=${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/install/lib/cmake/opencv4

# OpenVINO setup
ARG OV_REPO=https://github.com/openvinotoolkit/openvino.git
ARG OV_BRANCH=${OV_VERSION}.0
ARG OV_DIR=${WORKSPACE}/ivsr/ivsr_ov/based_on_openvino_${OV_VERSION}/openvino
WORKDIR ${OV_DIR}
RUN git config --global user.email "noname@example.com" && \
    git config --global user.name "no name" && \
    git clone ${OV_REPO} . --depth 1 -b ${OV_BRANCH}

RUN git submodule update --init --recursive

# Build OpenVINO
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=${PWD}/../install \
    -DENABLE_INTEL_CPU=ON -DENABLE_INTEL_GPU=ON -DENABLE_PYTHON=ON \
    -DENABLE_SYSTEM_TBB=ON \
    -DENABLE_SAMPLES=ON -DENABLE_CPPLINT=OFF -DENABLE_TESTS=OFF \
    -DENABLE_FUNCTIONAL_TESTS=OFF -DENABLE_DEBUG_CAPS=ON \
    -DENABLE_GPU_DEBUG_CAPS=ON -DENABLE_CPU_DEBUG_CAPS=ON \
    -DCMAKE_BUILD_TYPE=Release .. && \
    make -j16 && \
    make install && \
    bash ../install/setupvars.sh

# Environment variables for custom IE build
ARG CUSTOM_IE_DIR=${OV_DIR}/install/runtime
ENV OpenVINO_DIR=${CUSTOM_IE_DIR}/cmake
ENV InferenceEngine_DIR=${CUSTOM_IE_DIR}/cmake
ENV TBB_DIR=${CUSTOM_IE_DIR}/3rdparty/tbb/cmake
ENV ngraph_DIR=${CUSTOM_IE_DIR}/cmake
ENV LD_LIBRARY_PATH=${CUSTOM_IE_DIR}/3rdparty/tbb/lib:${CUSTOM_IE_DIR}/lib/intel64:$LD_LIBRARY_PATH

# Build iVSR SDK
COPY ./ivsr_sdk ${WORKSPACE}/ivsr/ivsr_sdk
WORKDIR ${WORKSPACE}/ivsr/ivsr_sdk
RUN mkdir -p build && cd build && \
    cmake .. -DENABLE_LOG=OFF -DENABLE_PERF=OFF -DENABLE_THREADPROCESS=ON -DCMAKE_BUILD_TYPE=Release && \
    make -j16 && \
    make install && \
    echo "Building iVSR SDK finished."

# Install FFmpeg dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    ca-certificates tar g++ wget pkg-config nasm yasm libglib2.0-dev flex bison gobject-introspection libgirepository1.0-dev \
    python3-dev libx11-dev libxv-dev libxt-dev libasound2-dev libpango1.0-dev libtheora-dev libvisual-0.4-dev libgl1-mesa-dev \
    libcurl4-gnutls-dev librtmp-dev mjpegtools libx264-dev libx265-dev libde265-dev libva-dev && \
    rm -rf /var/lib/apt/lists/*

# FFmpeg setup and build
ARG FFMPEG_REPO=https://github.com/FFmpeg/FFmpeg.git
ARG FFMPEG_VERSION=n6.1
ARG FFMPEG_IVSR_SDK_PLUGIN_DIR=${WORKSPACE}/ivsr/ivsr_ffmpeg_plugin
WORKDIR ${FFMPEG_IVSR_SDK_PLUGIN_DIR}/ffmpeg
RUN git clone ${FFMPEG_REPO} . && \
    git checkout ${FFMPEG_VERSION}
COPY ./ivsr_ffmpeg_plugin/patches/*.patch ./
RUN for patch_file in $(find -iname "*.patch" | sort -n); do \
    echo "Applying: ${patch_file}"; \
    git am --whitespace=fix ${patch_file}; \
    done

RUN sed -i 's|-L${prefix}/runtime/3rdparty/tbb|-L${prefix}/runtime/3rdparty/tbb/lib|' \
    ${CUSTOM_IE_DIR}/lib/intel64/pkgconfig/openvino.pc

# Configure and build FFmpeg
RUN source ${CUSTOM_IE_DIR}/../setupvars.sh && \
    export LD_LIBRARY_PATH=${WORKSPACE}/ivsr/ivsr_sdk/lib:"$LD_LIBRARY_PATH" && \
    ./configure --extra-cflags=-fopenmp --extra-ldflags=-fopenmp \
    --enable-libivsr --disable-static --disable-doc --enable-shared \
    --enable-vaapi --enable-gpl --enable-libx264 --enable-libx265 --enable-version3  && \
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

# Set working directory and command
WORKDIR ${WORKSPACE}
CMD ["/bin/bash"]
