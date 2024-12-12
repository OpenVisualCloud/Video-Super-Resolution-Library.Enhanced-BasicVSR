#!/bin/sh

enable_ov_patch="false"

# Default os_version set to ubuntu22
os_version="ubuntu22"

# Extract available OV versions from Dockerfile names and format them with "|"
available_versions=$(ls dockerfiles/${os_version}/ov*.dockerfile 2>/dev/null | grep -oP '(?<=ov)\d+\.\d+[a-z]*' | paste -sd '|')

# Default OV_VERSION set to the first available version
ov_version=$(echo $available_versions | awk -F '|' '{print $1}')

# Extract available OS versions from name of dockerfiles folder
available_os=$(ls dockerfiles 2>/dev/null | paste -sd '|')

# Function to print usage and exit with error
print_usage_and_exit() {
  echo "Usage: $0 --enable_ov_patch [true|false] --ov_version [${available_versions}] --os_version [${available_os}]"
  exit 1
}

# Parse the arguments
while [ $# -gt 0 ]; do
  case "$1" in
    --enable_ov_patch)
      shift
      value=$(echo $1 | tr '[:upper:]' '[:lower:]')
      if [ "$value" = "true" ] || [ "$value" = "false" ]; then
        enable_ov_patch=$value
      else
        print_usage_and_exit
      fi
      shift
      ;;
    --ov_version)
      shift
      if echo "$available_versions" | grep -qw "$1"; then
        ov_version=$1
      else
        print_usage_and_exit
      fi
      shift
      ;;
    --os_version)
      shift
      value=$(echo $1 | tr '[:upper:]' '[:lower]')
      if echo "$available_os" | grep -qw "$value"; then
        os_version="$value";
      else
        print_usage_and_exit
      fi
      shift
    ;;
    *)
      print_usage_and_exit
      ;;
  esac
done

# Configure ENABLE_OV_PATCH according to OV version
if [ "$ov_version" = "2022.3" ]; then
  echo "Setting ENABLE_OV_PATCH to $enable_ov_patch for version 2022.3."
else
  enable_ov_patch="false"
  echo "ENABLE_OV_PATCH is not applicable for version $ov_version. Automatically set to false."
fi

docker build \
    --build-arg http_proxy=$http_proxy \
    --build-arg https_proxy=$https_proxy \
    --build-arg no_proxy=$no_proxy \
    --build-arg PYTHON=python3.10 \
    --build-arg ENABLE_OV_PATCH=$enable_ov_patch \
    --build-arg OV_VERSION=$ov_version \
    -f ./dockerfiles/$os_version/ov${ov_version}.dockerfile \
    -t ffmpeg_ivsr_sdk_${os_version}_ov${ov_version} \
    ../
