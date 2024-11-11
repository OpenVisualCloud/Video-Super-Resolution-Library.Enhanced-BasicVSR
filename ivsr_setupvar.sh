#!/bin/bash
OV_VERSION="2022.3"
# Parse the --ov_version
while [ $# -gt 0 ]; do
    case "$1" in
        --ov_version)
            shift
            if [ "$1" = "2022.3" ] || [ "$1" = "2023.2" ]; then
                OV_VERSION=$1
            else
                echo "Usage: $0  --ov_version [2022.3|2023.2]"
                exit 1
            fi
            shift
            ;;
        *)
            echo "Usage: $0  --ov_version [2022.3|2023.2]"
            exit 1
            ;;
    esac
done
PROJECTDIR=${PWD}
IVSR_OV_DIR=${PROJECTDIR}/ivsr_ov/based_on_openvino_${OV_VERSION}/openvino
CUSTOM_OV_INSTALL_DIR=${IVSR_OV_DIR}/install
IVSR_SDK_DIR=${PROJECTDIR}/ivsr_sdk/
CUSTOM_IE_DIR=${CUSTOM_OV_INSTALL_DIR}/runtime
CUSTOM_IE_LIBDIR=${CUSTOM_IE_DIR}/lib/intel64
export OpenCV_DIR=${PROJECTDIR}/../opencv-4.5.3-openvino-2021.4.2/install/lib/cmake/opencv4
export LD_LIBRARY_PATH=${PROJECTDIR}/../opencv-4.5.3-openvino-2021.4.2/install/lib:${IVSR_SDK_DIR}/lib:${CUSTOM_IE_DIR}/3rdparty/tbb/lib:/usr/local/lib:${CUSTOM_IE_LIBDIR}:$LD_LIBRARY_PATH
bash ${CUSTOM_OV_INSTALL_DIR}/setupvars.sh
sudo ldconfig
