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

ARG IMAGE=rockylinux@sha256:d7be1c094cc5845ee815d4632fe377514ee6ebcf8efaed6892889657e5ddaaa6
FROM $IMAGE AS base

RUN dnf -y update && \
   dnf -y install \
   wget && \
   dnf clean all

FROM base as build
LABEL vendor="Intel Corporation"

RUN dnf -y install cmake \
   gcc \
   git \
   g++ && \
   dnf clean all

ARG WORKSPACE=/workspace

# install openvino
RUN tee /tmp/openvino-2024.repo <<EOF
[OpenVINO]
name=Intel(R) Distribution of OpenVINO 2024
baseurl=https://yum.repos.intel.com/openvino/2024
enabled=1
gpgcheck=1
repo_gpgcheck=1
gpgkey=https://yum.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB
EOF

RUN mv /tmp/openvino-2024.repo /etc/yum.repos.d && \
    yum -y install openvino-2024.5.0

RUN git config --global user.email "noname@example.com" && \
  git config --global user.name "no name"
# install ivsr sdk
ARG IVSR_DIR=${WORKSPACE}/ivsr
ARG IVSR_SDK_DIR=${IVSR_DIR}/ivsr_sdk/
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
