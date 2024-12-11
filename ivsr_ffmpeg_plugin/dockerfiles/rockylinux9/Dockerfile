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

ARG IMAGE=rockylinux:9.3
FROM $IMAGE AS base

RUN dnf -y update && \
   dnf -y install \
   wget && \
   dnf clean all

FROM base as build
LABEL vendor="Intel Corporation"

RUN dnf -y install cmake \
   gcc \
   g++ && \
   dnf clean all

ARG ENABLE_OV_PATCH
ARG OV_VERSION

# install opencv
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

ENV LD_LIBRARY_PATH=${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/install/lib:$LD_LIBRARY_PATH
ENV OpenCV_DIR=${WORKSPACE}/opencv-4.5.3-openvino-2021.4.2/install/lib/cmake/opencv4

# install openvino
RUN dnf -y install git \
   python3-devel && \
   dnf clean all
RUN dnf -y --enablerepo=crb install python3-Cython && \
   dnf clean all

ARG IVSR_DIR=${WORKSPACE}/ivsr
ARG IVSR_OV_DIR=${IVSR_DIR}/ivsr_ov/based_on_openvino_${OV_VERSION}/openvino
ARG CUSTOM_OV_INSTALL_DIR=${IVSR_OV_DIR}/install
ARG IVSR_SDK_DIR=${IVSR_DIR}/ivsr_sdk/

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
    -DENABLE_INTEL_GPU=OFF \
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
    -DENABLE_GPU_DEBUG_CAPS=OFF \
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

# install ivsr sdk
RUN dnf -y install zlib-devel
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
RUN dnf -y --enablerepo=crb install nasm
RUN dnf -y --enablerepo=devel install yasm
RUN dnf -y install diffutils

# build libx264
WORKDIR ${WORKSPACE}
RUN git clone https://github.com/mirror/x264 -b stable --depth 1 && \
    cd x264 && \
    ./configure --enable-shared && \
    make -j16 && \
    make install

# build libx265
WORKDIR ${WORKSPACE}
ARG LIBX265=https://github.com/videolan/x265/archive/3.4.tar.gz
RUN wget ${LIBX265} && \
    tar xzf ./3.4.tar.gz && \
    rm ./3.4.tar.gz && \
    cd x265-3.4/build/linux && \
    cmake -DBUILD_SHARED_LIBS=ON -DHIGH_BIT_DEPTH=ON ../../source && \
    make -j16 && \
    make install

ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
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

RUN ./configure \
--extra-cflags=-fopenmp \
--extra-ldflags=-fopenmp \
--enable-libivsr \
--disable-static \
--disable-doc \
--enable-shared \
--enable-gpl \
--enable-libx264 \
--enable-libx265 \
--enable-version3 && \
make -j16 && \
make install

WORKDIR ${WORKSPACE}
CMD ["/bin/bash"]
