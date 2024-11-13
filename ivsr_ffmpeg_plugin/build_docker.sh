#!/bin/sh
# Default value for the ENABLE_OV_PATCH flag
ENABLE_OV_PATCH="true"
OV_VERSION="2022.3"
OV_VERSION_N="ov2022.3"

# Parse the --enable_ov_patch flag and --ov_version
while [ $# -gt 0 ]; do
    case "$1" in
        --enable_ov_patch)
            shift
            value=$(echo $1 | tr '[:upper:]' '[:lower:]')
            if [ "$value" = "false" ]; then
                ENABLE_OV_PATCH=$value
            fi
            shift
            ;;
        --ov_version)
            shift
            if [ "$1" = "2022.3" ]; then
                OV_VERSION=$1
                OV_VERSION_N="ov2022.3"
            elif [ "$1" = "2023.2" ]; then
                OV_VERSION=$1
                OV_VERSION_N="ov2023.2"
            else
                echo "Usage: $0 --enable_ov_patch [true|false] --ov_version [2022.3|2023.2]"
                exit 1
            fi
            shift
            ;;
        *)
            echo "Usage: $0 --enable_ov_patch [true|false] --ov_version [2022.3|2023.2]"
            exit 1
            ;;
    esac
done

if [ "$OV_VERSION" = "2023.2" ]; then
    ENABLE_OV_PATCH="false"
    echo "There is no openvino patches for openvino 2023.2 version, will ignore the setting of ENABLE_OV_PATCH"
fi
docker build --build-arg http_proxy=$http_proxy \
 	--build-arg https_proxy=$https_proxy \
	--build-arg no_proxy=$no_proxy \
	--build-arg PYTHON=python3.10 \
	--build-arg ENABLE_OV_PATCH=$ENABLE_OV_PATCH \
    --build-arg OV_VERSION=$OV_VERSION \
	-f Dockerfile -t ffmpeg_ivsr_sdk_$OV_VERSION_N \
	../
