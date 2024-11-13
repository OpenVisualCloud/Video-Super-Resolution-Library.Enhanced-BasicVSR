#!/bin/bash
set -e

PROJECTDIR=${PWD}
# Function to display usage information
usage() {
    echo "Usage: $0  --enable_ov_patch [true|false]
                        --enable_compile_ffmpeg [true|false]
                        --ov_version [2022.3|2023.2]"
    exit 1
}



# Initialize variables
ENABLE_OV_PATCH="false"
ENABLE_COMPILE_FFMPEG="true"
OV_VERSION="2022.3"



# Parse command line arguments
while [ $# -gt 0 ]; do
  case "$1" in
    --enable_ov_patch)
      shift
      ENABLE_OV_PATCH=$(echo "$1" | tr '[:upper:]' '[:lower:]')
      if [ "$ENABLE_OV_PATCH" != "true" ] && [ "$ENABLE_OV_PATCH" != "false" ]; then
        usage
        exit 1
      fi
      ;;
    --enable_compile_ffmpeg)
      shift
      ENABLE_COMPILE_FFMPEG=$(echo "$1" | tr '[:upper:]' '[:lower:]')
      if [ "$ENABLE_COMPILE_FFMPEG" != "true" ] && [ "$ENABLE_COMPILE_FFMPEG" != "false" ]; then
        usage
        exit 1
      fi
      ;;
    --ov_version)
        shift
        if [ "$1" = "2022.3" ] || [ "$1" = "2023.2" ]; then
          OV_VERSION=$1
        else
          usage
          exit 1
        fi
        ;;
    *)
      # If the flag doesn't match any known option, display usage message
      usage
      ;;
  esac
  shift # Move to the next argument
done

if [ "$OV_VERSION" = "2023.2" ]; then
    ENABLE_OV_PATCH="false"
    echo "There is no openvino patches for openvino 2023.2 version, will ignore the setting of ENABLE_OV_PATCH"
fi



# 1.Install pre-requisites
apt-get update && DEBIAN_FRONTEND=noninteractive && apt-get install -y --no-install-recommends --fix-missing \
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
        patchelf \
        pkg-config \
        xz-utils \
        ocl-icd-opencl-dev \
        opencl-headers \
        apt-utils \
        gpg-agent \
        software-properties-common \
        wget \
        python3-dev libpython3-dev python3-pip

apt-get clean
pip --no-cache-dir install --upgrade pip setuptools
pip install numpy



# 2.Build and Install opencv
echo "Start building OpenCV"
OPENCV_REPO=https://github.com/opencv/opencv/archive/4.5.3-openvino-2021.4.2.tar.gz
cd ${PROJECTDIR}/.. && wget -qO - ${OPENCV_REPO} | tar xz
OPENCV_BASE=${PROJECTDIR}/../opencv-4.5.3-openvino-2021.4.2
cd ${OPENCV_BASE}  && mkdir -p build && mkdir -p install && cd build
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
  make install
cd ${OPENCV_BASE}/install/bin && bash ./setup_vars_opencv4.sh
echo "Build OpenCV finished."



# 3.install GPU runtime drivers
## 3.1 Add the online network repository of intel graphics.
rm -f /usr/share/keyrings/intel-graphics.gpg
no_proxy=$no_proxy wget -qO - https://repositories.intel.com/graphics/intel-graphics.key | gpg --dearmor --output /usr/share/keyrings/intel-graphics.gpg
echo 'deb [arch=amd64 signed-by=/usr/share/keyrings/intel-graphics.gpg] https://repositories.intel.com/graphics/ubuntu jammy flex' | \
    tee  /etc/apt/sources.list.d/intel.gpu.jammy.list

## 3.2-1 BKC for OV2022.3
if [ "$OV_VERSION" = "2022.3" ]; then
    apt-get update
    # repository to install Intel(R) GPU drivers
    xargs apt-get install -y --no-install-recommends --fix-missing < ${PROJECTDIR}/ivsr_sdk/dgpu_umd_stable_555_0124.txt
    apt-get install -y vainfo clinfo
    apt-get clean
fi

## 3.2-2 BKC for OV2023.2
if [ "$OV_VERSION" = "2023.2" ]; then
  apt-get update
  apt-get install -y vainfo clinfo
  apt-get install -y --no-install-recommends ocl-icd-libopencl1
  apt-get clean
  #hadolint ignore=DL3003
  mkdir /tmp/gpu_deps && cd /tmp/gpu_deps
  curl -L -O https://github.com/intel/compute-runtime/releases/download/23.05.25593.11/libigdgmm12_22.3.0_amd64.deb
  curl -L -O https://github.com/intel/intel-graphics-compiler/releases/download/igc-1.0.13700.14/intel-igc-core_1.0.13700.14_amd64.deb
  curl -L -O https://github.com/intel/intel-graphics-compiler/releases/download/igc-1.0.13700.14/intel-igc-opencl_1.0.13700.14_amd64.deb
  curl -L -O https://github.com/intel/compute-runtime/releases/download/23.13.26032.30/intel-opencl-icd_23.13.26032.30_amd64.deb
  curl -L -O https://github.com/intel/compute-runtime/releases/download/23.13.26032.30/libigdgmm12_22.3.0_amd64.deb
  dpkg -i ./*.deb
  rm -rf /tmp/gpu_deps
  cd ${PROJECTDIR}
fi



# 4. Build OpenVINO
if [ -z "$(git config --global user.name)" ]; then
    git config --global user.name "no name"
fi

if [ -z "$(git config --global user.email)" ]; then
    git config --global user.email "noname@example.com"
fi
echo "Start building OpenVINO"
export LD_LIBRARY_PATH=${OPENCV_BASE}/install/lib:$LD_LIBRARY_PATH
export OpenCV_DIR=${OPENCV_BASE}/install/lib/cmake/opencv4
OV_REPO=https://github.com/openvinotoolkit/openvino.git
OV_BRANCH=${OV_VERSION}.0
IVSR_OV_DIR=${PROJECTDIR}/ivsr_ov/based_on_openvino_${OV_VERSION}/openvino
git clone ${OV_REPO} ${IVSR_OV_DIR}
cd ${IVSR_OV_DIR}
git config --global --add safe.directory ${IVSR_OV_DIR}
git checkout ${OV_BRANCH}
git submodule update --init --recursive

## 4.1 applying ov22.3 patches to enable Enhanced BasicVSR model
if [ "$ENABLE_OV_PATCH" = "true" ] && [ "$OV_VERSION" = "2022.3" ]; then
    for patch_file in $(find ../patches -iname "*.patch" | sort -n);do
        echo "Applying: ${patch_file}"
        git am --whitespace=fix ${patch_file}
    done
fi

cd ${IVSR_OV_DIR}
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
bash ${PWD}/../install/setupvars.sh
echo "Build OpenVINO finished."



#5. Build iVSR-SDK
CUSTOM_OV_INSTALL_DIR=${IVSR_OV_DIR}/install
CUSTOM_IE_DIR=${CUSTOM_OV_INSTALL_DIR}/runtime
CUSTOM_IE_LIBDIR=${CUSTOM_IE_DIR}/lib/intel64
export OpenVINO_DIR=${CUSTOM_IE_DIR}/cmake
export InferenceEngine_DIR=${CUSTOM_IE_DIR}/cmake
export TBB_DIR=${CUSTOM_IE_DIR}/3rdparty/tbb/cmake
export ngraph_DIR=${CUSTOM_IE_DIR}/cmake
export LD_LIBRARY_PATH=${CUSTOM_IE_DIR}/3rdparty/tbb/lib:${CUSTOM_IE_LIBDIR}:$LD_LIBRARY_PATH

echo "Start building ivsr sdk."
IVSR_SDK_DIR=${PROJECTDIR}/ivsr_sdk/
mkdir -p ${IVSR_SDK_DIR}/build 
cd ${IVSR_SDK_DIR}/build 
cmake .. \
  -DENABLE_LOG=OFF -DENABLE_PERF=OFF -DENABLE_THREADPROCESS=ON \
  -DCMAKE_BUILD_TYPE=Release
make
echo "Build ivsr sdk finished."



#6. Build ffmpeg with iVSR SDK backend, disable it if you don't need it.
if ${ENABLE_COMPILE_FFMPEG}; then
  echo "Start building FFMPEG"
  apt-get update && \
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
    libva-dev
  export LD_LIBRARY_PATH=${IVSR_SDK_DIR}/lib:/usr/local/lib:$LD_LIBRARY_PATH
  FFMPEG_IVSR_SDK_PLUGIN_DIR=${PROJECTDIR}/ivsr_ffmpeg_plugin
  FFMPEG_DIR=${FFMPEG_IVSR_SDK_PLUGIN_DIR}/ffmpeg
  FFMPEG_REPO=https://github.com/FFmpeg/FFmpeg.git
  FFMPEG_VERSION=n6.1
  git clone ${FFMPEG_REPO} ${FFMPEG_DIR}
  cd ${FFMPEG_DIR}
  git config --global --add safe.directory ${FFMPEG_DIR}
  git checkout ${FFMPEG_VERSION}

  cp  ${PROJECTDIR}/ivsr_ffmpeg_plugin/patches/*.patch ${FFMPEG_DIR}/
  cd ${FFMPEG_DIR} && { set -e;
  for patch_file in $(find -iname "*.patch" | sort -n); do
    echo "Applying: ${patch_file}";
    git am --whitespace=fix ${patch_file};
  done; }
  if [ -f "${CUSTOM_OV_INSTALL_DIR}/setvars.sh" ]; then
    . ${CUSTOM_OV_INSTALL_DIR}/setvars.sh
  fi
  export LD_LIBRARY_PATH=${IVSR_SDK_DIR}/lib:${CUSTOM_IE_LIBDIR}:${TBB_DIR}/../lib:"$LD_LIBRARY_PATH"
  cd ${FFMPEG_DIR}
  ./configure \
  --enable-libivsr \
  --extra-cflags=-I${IVSR_SDK_DIR}/include/ \
  --extra-ldflags=-L${IVSR_SDK_DIR}/lib \
  --disable-static \
  --disable-doc \
  --enable-shared \
  --enable-vaapi \
  --enable-gpl \
  --enable-libx264 \
  --enable-libx265 \
  --enable-version3
  make -j $(nproc --all)
  make install
  echo "Build FFMPEG finished."
fi

