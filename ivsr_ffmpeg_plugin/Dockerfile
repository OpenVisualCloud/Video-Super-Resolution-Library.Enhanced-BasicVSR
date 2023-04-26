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

ARG IMAGE=ubuntu:20.04
FROM $IMAGE AS base

RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    curl wget ca-certificates gpg-agent software-properties-common && \
  rm -rf /var/lib/apt/lists/*

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

FROM base as build
LABEL vendor="Intel Corporation"

RUN mkdir -p /workspace/ivsr

# openvino
RUN apt-get update && \
        DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
            ca-certificates \
            curl \
            cmake \
            cython3 \
            flex \
            bison \
            gcc \
            g++ \
            git \
            libdrm-dev \
            libudev-dev \
            python-gi-dev \
            libboost-all-dev \
            libgtk2.0-dev \
            libgtk-3-dev \
            libtool \
            libusb-1.0-0-dev \
            make \
            patch \
            python3 \
            pkg-config \
            python3-yaml \
            xz-utils \
            ocl-icd-opencl-dev \
            opencl-headers && \
    rm -rf /var/lib/apt/lists/*

#install opencv
ARG WORKSPACE=/workspace
ARG OPENCV_REPO=https://github.com/opencv/opencv/archive/4.5.3-openvino-2021.4.2.tar.gz
RUN cd ${WORKSPACE} && \
  wget -qO - ${OPENCV_REPO} | tar xz

RUN cd ${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2 && mkdir build && mkdir install && cd build && \
  cmake \
    -DCMAKE_BUILD_TYPE=Release \
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
  make -j "$(nproc)" && \
  make install && \
  cd ${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/install/bin && bash ./setup_vars_opencv4.sh

ENV LD_LIBRARY_PATH=${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/install/lib:$LD_LIBRARY_PATH
ENV OpenCV_DIR=${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/install/lib/cmake/opencv4
ARG IVSR_DIR=${WORKSPACE}/ivsr
ARG IVSR_OV1_GPU_OPT_DIR=${IVSR_DIR}/ivsr_gpu_opt/based_on_openvino_2022.1
ARG CUSTOM_OV_INSTALL_DIR=${IVSR_DIR}/ivsr_gpu_opt/based_on_openvino_2022.1/openvino/install

COPY . $IVSR_DIR
RUN git config --global user.email "noname@example.com" && \
  git config --global user.name "no name" && \
  cd ${IVSR_OV1_GPU_OPT_DIR} && \
  bash build.sh

ARG CUSTOM_IE_DIR=${CUSTOM_OV_INSTALL_DIR}/runtime
ARG CUSTOM_IE_LIBDIR=${CUSTOM_IE_DIR}/lib/intel64
ENV CUSTOM_OV=${CUSTOM_IE_DIR}

ENV OpenVINO_DIR=${CUSTOM_IE_DIR}/cmake
ENV InferenceEngine_DIR=${CUSTOM_IE_DIR}/cmake
ENV TBB_DIR=${CUSTOM_IE_DIR}/3rdparty/tbb/cmake
ENV ngraph_DIR=${CUSTOM_IE_DIR}/cmake

RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates tar g++ wget pkg-config nasm libglib2.0-dev flex bison gobject-introspection libgirepository1.0-dev python3-dev \
    libx11-dev \
    libxv-dev \
    libxt-dev \
    libasound2-dev \
    libpango1.0-dev \
    libtheora-dev \
    libvisual-0.4-dev \
    libgl1-mesa-dev \
    libcurl4-gnutls-dev \
    librtmp-dev \
    mjpegtools \
    libx264-dev \
    libx265-dev \
    libde265-dev \
    libva-dev \
    && \
  rm -rf /var/lib/apt/lists/*

# build ffmepg
ARG FFMPEG_PLUGIN_DIR=${IVSR_DIR}/ivsr_ffmpeg_plugin
ARG FFMPEG_DIR=${FFMPEG_PLUGIN_DIR}/ffmpeg
RUN cd ${FFMPEG_PLUGIN_DIR} && \
  cp ${FFMPEG_PLUGIN_DIR}/patches/*.patch ${FFMPEG_DIR}/ && \
  cd ${FFMPEG_DIR} && { set -e; \
  for patch_file in $(find -iname "*.patch" | sort -n); do \
    echo "Applying: ${patch_file}"; \
    patch -p1 < "${patch_file}"; \
  done; }

RUN if [ -f "${CUSTOM_OV_INSTALL_DIR}/setvars.sh" ]; then \
      . ${CUSTOM_OV_INSTALL_DIR}/setvars.sh ; \
    fi && \
    export LD_LIBRARY_PATH=${CUSTOM_IE_LIBDIR}:${TBB_DIR}/../lib:"$LD_LIBRARY_PATH" && \
    cd ${FFMPEG_DIR} && \
    ./configure \
    --enable-libopenvino \
    --extra-cflags=-I${CUSTOM_IE_DIR}/include/ie \
    --extra-ldflags=-L${CUSTOM_IE_LIBDIR} \
    --extra-ldflags=-L${CUSTOM_OV_INSTALL_DIR}/runtime/3rdparty/tbb/lib/ \
    --disable-static \
    --disable-doc \
    --enable-shared \
    --enable-vaapi \
    --enable-gpl \
    --enable-libx264 \
    --enable-libx265 \
    --enable-version3 && \
    make -j $(nproc --all) && \
    make install

WORKDIR ${WORKSPACE}
CMD ["/bin/bash"]
