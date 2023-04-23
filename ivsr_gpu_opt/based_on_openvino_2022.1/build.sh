#!/bin/sh
EX_PATH=${PWD}
git submodule update --init ./openvino
cd openvino
git submodule update --init --recursive

cp ../patches/*.patch ./
for patch_file in $(find -iname "*.patch" | sort -n);
do
    echo "Applying: ${patch_file}";
    git am --whitespace=fix ${patch_file};
done

if [ ! -d "./build" ];then
    mkdir -p build
fi
cd build

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
-DCMAKE_BUILD_TYPE=Release ..

make -j`nproc`

make install

echo "Building OV22.1 finished."
