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

ARG IMAGE=ubuntu:22.04
FROM $IMAGE AS base

RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    curl wget ca-certificates gpg-agent software-properties-common && \
  rm -rf /var/lib/apt/lists/*

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

FROM base as build
LABEL vendor="Intel Corporation"

RUN mkdir -p /workspace/ivsr

ARG ENABLE_OV_PATCH
ARG OV_VERSION

# openvino
RUN apt-get update && \
        DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends --fix-missing \
            apt-utils \
            ca-certificates \
            curl \
            cmake \
            cython3 \
            flex \
            bison \
            gcc \
            g++ \
            git \
            make \
            patch \
            pkg-config \
            wget && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

RUN no_proxy=$no_proxy wget -qO - https://repositories.intel.com/graphics/intel-graphics.key | \
    gpg --dearmor --output /usr/share/keyrings/intel-graphics.gpg
RUN echo 'deb [arch=amd64 signed-by=/usr/share/keyrings/intel-graphics.gpg] https://repositories.intel.com/graphics/ubuntu jammy flex' | \
    tee  /etc/apt/sources.list.d/intel.gpu.jammy.list

#install opencv
ARG WORKSPACE=/workspace
ARG OPENCV_REPO=https://github.com/opencv/opencv/archive/4.5.3-openvino-2021.4.2.tar.gz
WORKDIR ${WORKSPACE}
RUN wget -qO - ${OPENCV_REPO} | tar xz

WORKDIR ${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2
RUN mkdir build && mkdir install
WORKDIR ${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/build
RUN cmake \
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
    make -j16 && \
    make install

WORKDIR ${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/install/bin
RUN bash ./setup_vars_opencv4.sh

COPY ./ivsr_sdk/dgpu_umd_stable_555_0124.txt ${WORKSPACE}/
RUN if [ "$OV_VERSION" = "2022.3" ]; then \
        apt-get update; \
        xargs apt-get install -y --no-install-recommends --fix-missing < ${WORKSPACE}/dgpu_umd_stable_555_0124.txt; \
        apt-get install -y vainfo clinfo; \
        apt-get clean; \
        rm -rf  /var/lib/apt/lists/*; \
    fi
RUN rm ${WORKSPACE}/dgpu_umd_stable_555_0124.txt

WORKDIR /tmp/gpu_deps
RUN if [ "$OV_VERSION" = "2023.2" ]; then \
      # for GPU
      apt-get update; \
      apt-get install -y vainfo clinfo; \
      apt-get install -y --no-install-recommends ocl-icd-libopencl1; \
      apt-get clean; \
      rm -rf /var/lib/apt/lists/* && rm -rf /tmp/*; \
    fi
WORKDIR /tmp/gpu_deps
RUN if [ "$OV_VERSION" = "2023.2" ]; then \
      # hadolint ignore=DL3003
      curl -L -O https://github.com/intel/compute-runtime/releases/download/23.05.25593.11/libigdgmm12_22.3.0_amd64.deb; \
      curl -L -O https://github.com/intel/intel-graphics-compiler/releases/download/igc-1.0.13700.14/intel-igc-core_1.0.13700.14_amd64.deb; \
      curl -L -O https://github.com/intel/intel-graphics-compiler/releases/download/igc-1.0.13700.14/intel-igc-opencl_1.0.13700.14_amd64.deb; \
      curl -L -O https://github.com/intel/compute-runtime/releases/download/23.13.26032.30/intel-opencl-icd_23.13.26032.30_amd64.deb; \
      curl -L -O https://github.com/intel/compute-runtime/releases/download/23.13.26032.30/libigdgmm12_22.3.0_amd64.deb; \
      dpkg -i ./*.deb && rm -Rf /tmp/gpu_deps; \
    fi

ENV LD_LIBRARY_PATH=${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/install/lib:$LD_LIBRARY_PATH
ENV OpenCV_DIR=${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/install/lib/cmake/opencv4
ARG IVSR_DIR=${WORKSPACE}/ivsr
ARG IVSR_OV_DIR=${IVSR_DIR}/ivsr_ov/based_on_openvino_${OV_VERSION}/openvino
ARG CUSTOM_OV_INSTALL_DIR=${IVSR_OV_DIR}/install
ARG IVSR_SDK_DIR=${IVSR_DIR}/ivsr_sdk/

RUN apt-get update && \
        DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
            build-essential \
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
            libtool \
            libusb-1.0-0-dev \
            make \
            patch \
            pkg-config \
            xz-utils \
            ocl-icd-opencl-dev \
            opencl-headers && \
    rm -rf /var/lib/apt/lists/*

ARG PYTHON

RUN apt-get update && apt-get install -y --no-install-recommends --fix-missing \
    ${PYTHON} lib${PYTHON}-dev python3-pip && \
    apt-get clean && \
    rm -rf  /var/lib/apt/lists/*

RUN pip --no-cache-dir install --upgrade \
    pip \
    setuptools

RUN ln -sf $(which ${PYTHON}) /usr/local/bin/python && \
    ln -sf $(which ${PYTHON}) /usr/local/bin/python3 && \
    ln -sf $(which ${PYTHON}) /usr/bin/python && \
    ln -sf $(which ${PYTHON}) /usr/bin/python3

ARG OV_REPO=https://github.com/openvinotoolkit/openvino.git
ARG OV_BRANCH=${OV_VERSION}.0
WORKDIR ${IVSR_OV_DIR}

RUN git config --global user.email "noname@example.com" && \
  git config --global user.name "no name"

RUN git clone ${OV_REPO} ${IVSR_OV_DIR} && \
    git checkout ${OV_BRANCH} && \
    git submodule update --init --recursive

COPY ./ivsr_ov/based_on_openvino_2022.3/patches/*.patch ${IVSR_OV_DIR}/../patches/
RUN if [ "$ENABLE_OV_PATCH" = "true" ] && [ "$OV_VERSION" = "2022.3" ]; then \
        { set -e; \
            for patch_file in $(find ../patches -iname "*.patch" | sort -n); do \
            echo "Applying: ${patch_file}"; \
            git am --whitespace=fix ${patch_file}; \
            done; }; \
    fi
RUN rm -rf ${IVSR_OV_DIR}/../patches

WORKDIR ${IVSR_OV_DIR}/build
RUN  cmake \
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
  make -j16 && \
  make install && \
  bash ${PWD}/../install/setupvars.sh

ARG CUSTOM_IE_DIR=${CUSTOM_OV_INSTALL_DIR}/runtime
ARG CUSTOM_IE_LIBDIR=${CUSTOM_IE_DIR}/lib/intel64
ARG CUSTOM_OV=${CUSTOM_IE_DIR}

ENV OpenVINO_DIR=${CUSTOM_IE_DIR}/cmake
ENV InferenceEngine_DIR=${CUSTOM_IE_DIR}/cmake
ENV TBB_DIR=${CUSTOM_IE_DIR}/3rdparty/tbb/cmake
ENV ngraph_DIR=${CUSTOM_IE_DIR}/cmake
ENV LD_LIBRARY_PATH=${CUSTOM_IE_DIR}/3rdparty/tbb/lib:${CUSTOM_IE_LIBDIR}:$LD_LIBRARY_PATH

COPY ./ivsr_sdk ${IVSR_SDK_DIR}
RUN echo ${IVSR_SDK_DIR}
WORKDIR ${IVSR_SDK_DIR}/build
RUN cmake .. \
      -DENABLE_LOG=OFF -DENABLE_PERF=OFF -DENABLE_THREADPROCESS=ON \
      -DCMAKE_BUILD_TYPE=Release && \
    make -j16 && \
    make install && \
    echo "Building vsr sdk finished."

#build ffmpeg with iVSR SDK backend
RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates tar g++ wget pkg-config nasm yasm libglib2.0-dev flex bison gobject-introspection libgirepository1.0-dev python3-dev \
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
ENV LD_LIBRARY_PATH=${IVSR_SDK_DIR}/lib:/usr/local/lib:$LD_LIBRARY_PATH

ARG FFMPEG_IVSR_SDK_PLUGIN_DIR=${IVSR_DIR}/ivsr_ffmpeg_plugin
ARG FFMPEG_DIR=${FFMPEG_IVSR_SDK_PLUGIN_DIR}/ffmpeg

ARG FFMPEG_REPO=https://github.com/FFmpeg/FFmpeg.git
ARG FFMPEG_VERSION=n6.1
WORKDIR ${FFMPEG_DIR}
RUN git clone ${FFMPEG_REPO} ${FFMPEG_DIR} && \
    git checkout ${FFMPEG_VERSION}
COPY ./ivsr_ffmpeg_plugin/patches/*.patch ${FFMPEG_DIR}/
RUN { set -e; \
  for patch_file in $(find -iname "*.patch" | sort -n); do \
    echo "Applying: ${patch_file}"; \
    git am --whitespace=fix ${patch_file}; \
  done; }

RUN if [ -f "${CUSTOM_OV_INSTALL_DIR}/setvars.sh" ]; then \
      . ${CUSTOM_OV_INSTALL_DIR}/setvars.sh ; \
    fi && \
    export LD_LIBRARY_PATH=${IVSR_SDK_DIR}/lib:${CUSTOM_IE_LIBDIR}:${TBB_DIR}/../lib:"$LD_LIBRARY_PATH" && \
    ./configure \
    --extra-cflags=-fopenmp \
    --extra-ldflags=-fopenmp \
    --enable-libivsr \
    --disable-static \
    --disable-doc \
    --enable-shared \
    --enable-vaapi \
    --enable-gpl \
    --enable-libx264 \
    --enable-libx265 \
    --enable-version3 && \
    make -j16 && \
    make install

WORKDIR ${WORKSPACE}
CMD ["/bin/bash"]
