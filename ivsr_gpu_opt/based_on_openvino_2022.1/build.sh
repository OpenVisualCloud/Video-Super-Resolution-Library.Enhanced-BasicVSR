#!/bin/sh
EX_PATH=${PWD}
git submodule update --init ./openvino
cd openvino
git submodule update --init --recursive
git am --whitespace=fix ${EX_PATH}/patches/0001-Vsr-opt-dev-2.patch
git am --whitespace=fix ${EX_PATH}/patches/0002-remove-the-onednn-folder-by-incorrect-submit-4.patch
git am --whitespace=fix ${EX_PATH}/patches/0003-BasicVSR-inference-sample-code-in-python-5.patch
git am --whitespace=fix ${EX_PATH}/patches/0004-flow_warp-custom-op-ocl-implementation-and-applicati.patch
git am --whitespace=fix ${EX_PATH}/patches/0005-Remove-cached-onednn.-8.patch
git am --whitespace=fix ${EX_PATH}/patches/0006-Enable-BasicVSR-model-conversion-from-pytorch-to-onn.patch
git am --whitespace=fix ${EX_PATH}/patches/0007-modification-of-custom-op-for-MO-10.patch
git am --whitespace=fix ${EX_PATH}/patches/0008-Reorder-optimization-9.patch
git am --whitespace=fix ${EX_PATH}/patches/0009-remove-debug-information-11.patch
git am --whitespace=fix ${EX_PATH}/patches/0010-Add-license.-12.patch
git am --whitespace=fix ${EX_PATH}/patches/0011-Add-nif-width-height-arguments-to-pytorch2onnx.py-to.patch
git am --whitespace=fix ${EX_PATH}/patches/0012-add-refined-cpp-sample-of-BasicVSR-14.patch
git am --whitespace=fix ${EX_PATH}/patches/0013-clean-useless-files-and-complement-the-introduction-.patch
git am --whitespace=fix ${EX_PATH}/patches/0014-add-lisense-header-16.patch
git am --whitespace=fix ${EX_PATH}/patches/0015-restore-file-mode-to-644-17.patch
git am --whitespace=fix ${EX_PATH}/patches/0016-Vsr-opt-dev-20.patch
git am --whitespace=fix ${EX_PATH}/patches/0017-Add-3rd-party-license-and-modify-readme.-21.patch
git am --whitespace=fix ${EX_PATH}/patches/0001-Bug-Fix-Modified-C-warp-extension-implementation-to-.patch
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
