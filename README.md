## iVSR

[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/OpenVisualCloud/iVSR/badge)](https://api.securityscorecards.dev/projects/github.com/OpenVisualCloud/iVSR)
[![OpenSSF Best Practices](https://bestpractices.coreinfrastructure.org/projects/9795/badge)](https://bestpractices.coreinfrastructure.org/projects/9795)
[![Dependency Review](https://github.com/OpenVisualCloud/iVSR/actions/workflows/dependency-review.yml/badge.svg)](https://github.com/OpenVisualCloud/iVSR/actions/workflows/dependency-review.yml)
[![CodeQL](https://github.com/OpenVisualCloud/iVSR/actions/workflows/codeql.yml/badge.svg)](https://github.com/OpenVisualCloud/iVSR/actions/workflows/codeql.yml)
[![License](https://img.shields.io/badge/license-BSD_3_Clause-stable.svg)](https://github.com/OpenVisualCloud/iVSR/blob/master/LICENSE.md)
[![Contributions](https://img.shields.io/badge/contributions-welcome-blue.svg)](https://github.com/OpenVisualCloud/iVSR/wiki)
[![Ubuntu-DockerFile-Build](https://github.com/OpenVisualCloud/iVSR/actions/workflows/ubuntu-build-docker.yml/badge.svg)](https://github.com/OpenVisualCloud/iVSR/actions/workflows/ubuntu-build-docker.yml)
[![Trivy](https://github.com/OpenVisualCloud/iVSR/actions/workflows/trivy.yml/badge.svg)](https://github.com/OpenVisualCloud/iVSR/actions/workflows/trivy.yml)

# Contents Overview
1. [Overview of iVSR](#1-overview-of-ivsr)
    - [What is iVSR](#11-what-is-ivsr)
    - [Why is iVSR needed](#12-why-is-ivsr-needed)
    - [iVSR Components](#13-ivsr-components)
    - [Capabilities of iVSR](#14-capabilities-of-ivsr)
2. [Setup iVSR env on linux](#2-setup-ivsr-env-on-linux)
    - [Install GPU in kernel space ](#21-optional-install-gpu-kernel-packages)
    - [Install dependencies and build iVSR manually](#22-install-dependencies-and-build-ivsr-manually)
    - [Install dependencies and build iVSR by scripts](#23-install-dependencies-and-build-ivsr-by-scripts)
    - [Install dependencies and build iVSR by Docker file](#24-install-dependencies-and-build-ivsr-by-docker-file)
3. [How to use iVSR](#3-how-to-use-ivsr)
    - [Run with iVSR SDK sample](#31-run-with-ivsr-sdk-sample)
    - [Run with FFmpeg](#32-run-with-ffmpeg)
4. [Model files](#4-model-files)
5. [License](#5-license)
# 1. Overview of iVSR
## 1.1 What is iVSR
iVSR aims to facilitate AI media processing with exceptional quality and performance on Intel's hardware.

iVSR offers a patch-based, heterogeneous, multi-GPU, and multi-algorithm solution, 
harnessing the full capabilities of Intel's CPUs and GPUs. 
And iVSR is adaptable for deployment on a single device, a distributed system, cloud infrastructure, edge cloud, or K8S environment.

<!-- ![overview](./docs/figs/iVSR.png) -->
<div align=center>
<img src="./docs/figs/iVSR.png" width = 75% height = 75% />
</div>

## 1.2 Why is iVSR needed

- Simple APIs are provided, ensuring that any changes to the OpenVINO API remain hidden.
- A patch-based solution is offered to facilitate inference on hardware with limited memory capacity. This is particularly useful for super-resolution of high-resolution input videos, such as 4K.
- The iVSR SDK includes features to safeguard AI models created by Intel, which contain Intel IP.
- The iVSR SDK is versatile and can support a wide range of AI media processing algorithms.
- For specific algorithms, performance optimization can be executed to better align with customer requirements.

## 1.3 iVSR Components
This repository or package includes the following major components:

### 1.3.1 iVSR SDK
The iVSR SDK is a middleware library that supports various AI video processing filters. It is designed to accommodate different AI inference backends, although currently, it only supports OpenVINO.<br>
For a detailed introduction to the iVSR SDK API, please refer to [this introduction](./ivsr_sdk/README.md#api-introduction).We've also included a `vsr_sample` as a demonstration of its usage.<br>

### 1.3.2 iVSR FFmpeg plugin
In order to support the widely-used media processing solution FFmpeg, we've provided an iVSR SDK plugin to simplify integration.<br>
This plugin is integrated into FFmpeg's [`dnn_processing` filter](https://ffmpeg.org/ffmpeg-filters.html#dnn_005fprocessing-1) in the libavfilter library, serving as a new `ivsr` backend to this filter. Please note that the patches provided in this project are specifically for FFmpeg n6.1.<br>

### 1.3.3 OpenVINO patches and extension
In [this folder](./ivsr_ov/based_on_openvino_2022.3/patches), you'll find patches for OpenVINO that enable the Enhanced BasicVSR model. These patches utilize OpenVINO's [Custom OpenVINO™ Operations](https://docs.openvino.ai/latest/openvino_docs_Extensibility_UG_add_openvino_ops.html) feature, which allows users to support models with custom operations not inherently supported by OpenVINO.<br>
These patches are specifically for OpenVINO 2022.3, meaning the Enhanced BasicVSR model will only work on OpenVINO 2022.3 with these patches applied.<br>


## 1.4 Capabilities of iVSR
Currently, iVSR offers two AI media processing functionalities: Video Super Resolution (VSR), and Smart Video Processing (SVP) for bandwidth optimization. Both functionalities can be run on Intel CPUs and Intel GPUs (including Flex170, Arc770) via OpenVINO and FFmpeg.


### 1.4.1 Video Super Resolution (VSR)
Video Super Resolution (VSR) is a technique extensively employed in the AI media enhancement domain to upscale low-resolution videos to high-resolution. iVSR supports `Enhanced BasicVSR`, `Enhanced EDSR`, `TSENet`, and has the capability to be extended to support additional models.

- ####  i. Enhanced BasicVSR
    `BasicVSR` is a publicly available AI-based VSR algorithm. For more details on the public `BasicVSR`, please refer to this [paper](https://arxiv.org/pdf/2012.02181.pdf).<br>
    We have improved the public model to attain superior visual quality and reduced computational complexity,  named `Enhanced BasicVSR`. The performance of the `Enhanced BasicVSR` model inference has also been optimized for Intel GPUs. Please note that this optimization is specific to OpenVINO 2022.3. Therefore, the Enhanced BasicVSR model only works with OpenVINO 2022.3 with the applied patches.

-  #### ii. Enhanced EDSR
    `EDSR` is another publicly available AI-based single image SR algorithm. For more details on the public EDSR, please refer to this [paper](https://arxiv.org/pdf/1707.02921.pdf)

    We have improved the public `EDSR` model to reduce the computational complexity by over 79% compared to Enhanced BasicVSR, while maintaining similar visual quality, named `Enhanced EDSR`.

- #### iii. TSENet
  `TSENet` is one multi-frame SR algorithm derived from [ETDS](https://github.com/ECNUSR/ETDS).<br>
  We provide a preview version of the feature to support this model in the SDK and its plugin. Please contact your Intel representative to obtain the model package.

### 1.4.2. Smart Video Processing (SVP)
`SVP` is an AI-based video prefilter that enhances the perceptual rate-distortion in video encoding. With `SVP`, the encoded video streams maintain the same visual quality while reducing bandwidth, as measured by common video quality metrics (such as VMAF and (MS-)SSIM) and human perception.

# 2. Setup iVSR env on linux
The software was validated on:
- Intel Xeon hardware platform
- (Optional) Intel Data Center GPU Flex 170(*aka* ATS-M1 150W)
- Host OS: Linux based OS (Ubuntu 22.04)
- Docker OS: Ubuntu 22.04
- OpenVINO: [2022.3](https://github.com/openvinotoolkit/openvino/tree/2022.3.0) or [2023.2](https://github.com/openvinotoolkit/openvino/tree/2023.2.0)
- FFmpeg: [n6.1](https://github.com/FFmpeg/FFmpeg/tree/n6.1)

Building iVSR requires the installation of the GPU driver(optional), OpenCV, OpenVINO, and FFmpeg.<br>
We provide **three** ways to install requirements and build iVSR SDK & iVSR FFmpeg plugin:<br>
1. [Install dependencies and build iVSR manually](#22-install-dependencies-and-build-ivsr-manually)<br>
2. [Install dependencies and build iVSR by scripts](#23-install-dependencies-and-build-ivsr-by-scripts)<br>
3. [Install dependencies and build iVSR by Docker file](#24-install-dependencies-and-build-ivsr-by-docker-file)<br>

Note that to run inference on a **GPU**, it is necessary to have **kernel packages** installed on the bare metal system beforehand. See [Install GPU kernel packages ](#21-optional-install-gpu-kernel-packages) for details.<br>

## 2.1 (Optional) Install GPU kernel packages
Refer to this [instruction](https://dgpu-docs.intel.com/driver/installation.html#ubuntu-package-installation) for the installation guide on Ubuntu. GPU runtime driver/packages are also installed in script and dockerfile provided.

## 2.2 Install dependencies and build iVSR manually

### 2.2.1 (Optional) Install software for Intel® Data Center GPU Flex Series
To facilitate inference on Intel Data Center GPU, it's necessary to have both the kernel driver and the run-time driver and software installed. If you're planning to run inference on a CPU only, you can disregard this step.<br>

The detailed installation instruction is on [this page](https://dgpu-docs.intel.com/driver/installation.html#).<br>


### 2.2.2 Install OpenCV
OpenCV, which is used by the iVSR SDK sample for image processing tasks, needs to be installed. Detailed installation instructions can be found at [Installation OpenCV in Linux](https://docs.opencv.org/4.x/d7/d9f/tutorial_linux_install.html).<br>

### 2.2.3 Install OpenVINO
OpenVINO, currently the only backend supported by iVSR for model inference, should also be installed. You can refer to this [instruction](https://github.com/openvinotoolkit/openvino/blob/master/docs/dev/build_linux.md) to build OpenVINO from the source code.<br>

### 2.2.4 Build iVSR SDK
Once the dependencies are installed in the system, you can proceed to build the iVSR SDK and its sample.<br>
```bash
source <OpenVINO installation dir>/install/setupvars.sh
export OpenCV_DIR=<OpenCV installation dir>/install/lib/cmake/opencv4
cd ivsr_sdk
mkdir -p ./build
cd ./build
cmake .. -DENABLE_THREADPROCESS=ON -DCMAKE_BUILD_TYPE=Release
make
```
### 2.2.5 Build FFmpeg with iVSR plugin
We provide patches specifically for FFmpeg n6.1. Apply these patches as instructed below:<br>
```bash
git clone https://github.com/FFmpeg/FFmpeg.git ./ivsr_ffmpeg_plugin/ffmpeg
cd ./ivsr_ffmpeg_plugin/ffmpeg
git checkout n6.1
cp ../patches/*.patch ./
for patch_file in $(find -iname "*.patch" | sort -n); do \
    echo "Applying: ${patch_file}";                      \
    git am --whitespace=fix ${patch_file};               \
done;
```
Finally, build FFmpeg. You can also enable other FFmpeg plugins as per the instructions provided in the [Compile FFmpeg for Ubuntu](https://trac.ffmpeg.org/wiki/CompilationGuide/Ubuntu) guide.<br>
```bash
source <OpenVINO installation folder>/install/setupvars.sh
./configure --enable-libivsr --extra-cflags=-I<Package_DIR>/ivsr_sdk/include/ --extra-ldflags=-L<Package_DIR>/ivsr_sdk/lib
make -j $(nproc --all)
make install
```
## 2.3 Install dependencies and build iVSR by scripts
We provide shell scripts `build_ivsr.sh` and `ivsr_setupvar.sh` to assist in building the dependencies from source code and setting up the environment from scratch.<br>
```bash
#ivsr environment building
chmod a+x ./build_ivsr.sh
sudo ./build_ivsr.sh --enable_ov_patch <true/false> --enable_compile_ffmpeg true --ov_version <2022.3|2023.2>
#environment variables setting
source ./ivsr_setupvar.sh --ov_version <2022.3|2023.2>
```
The scripts accept the following input parameters:<br>
- `enable_ov_patch`: Determines whether to enable OpenVINO patches, which are necessary to run the Enhanced BasicVSR model.<br>
- `enable_compile_ffmpeg`: Determines whether to compile FFmpeg. Set this to `false` if you're only using the iVSR SDK sample.<br>
- `ov_version`: Specifies the OpenVINO version. iVSR supports `2022.3` & `2023.2`. Note that running the Enhanced BasicVSR model requires `2022.3`.<br>

Feel free to modify and update these scripts as per your requirements.<br>


## 2.4 Install dependencies and build iVSR by Docker file.
A Dockerfile is also provided to expedite the environment setup process. Follow the steps below to build the docker image and run the docker container.<br>

### 2.4.1. Set timezone correctly before building docker image.
The following command takes Shanghai as an example.

  ```bash
  timedatectl set-timezone Asia/Shanghai
  ```

### 2.4.2 Set up docker service

```bash
sudo mkdir -p /etc/systemd/system/docker.service.d
printf "[Service]\nEnvironment=\"HTTPS_PROXY=$https_proxy\" \"NO_PROXY=$no_proxy\"\n" | sudo tee  /etc/systemd/system/docker.service.d/proxy.conf
sudo systemctl daemon-reload
sudo systemctl restart docker
```

### 2.4.3 Build docker image

```bash
cd ./ivsr_ffmpeg_plugin
./build_docker.sh --enable_ov_patch [true|false] --ov_version [2022.3|2023.2]
```
- `enable_ov_patch`: Set as `true` or `flase` to enable or disable the application of OpenVINO 2022.3 patches, which are needed to support the Enhanced BasicVSR model.<br>
- `ov_version`: Set the OpenVINO version to `2022.3` or `2023.2`, which will be built and installed. iVSR currently supports both OpenVINO 2022.3 and 2023.2, but the patches to enable the Enhanced BasicVSR model are only for OpenVINO 2022.3.<br>
If the docker image builds successfully, you can see a docker image named `ffmpeg_ivsr_sdk_ov2022.3` or `ffmpeg_ivsr_sdk_ov2023.2` in the output of `docker image ls`.<br>

### 2.4.4. Start Docker Container

```bash
sudo docker run -itd --name ffmpeg_ivsr_sdk_container --privileged -e MALLOC_CONF="oversize_threshold:1,background_thread:true,metadata_thp:auto,dirty_decay_ms:9000000000,muzzy_decay_ms:9000000000" -e http_proxy=$http_proxy -e https_proxy=$https_proxy -e no_proxy=$no_proxy --shm-size=128g --device=/dev/dri:/dev/dri ffmpeg_ivsr_sdk_[ov2022.3|ov2023.2]:latest bash
sudo docker exec -it ffmpeg_ivsr_sdk_container bash
```
Note `--device=/dev/dri:/dev/dri` is specified in the command to add the host gpu device to container.<br>

# 3. How to use iVSR
Both `vsr_sample` and FFmpeg integration are provided to run inference on the iVSR SDK. Execute the following commands to setup the env before executing them. <br>
```bash
source <OpenVINO installation dir>/install/setupvars.sh
export LD_LIBRARY_PATH=<Package dir>/ivsr_sdk/lib:<OpenCV installation folder>/install/lib:$LD_LIBRARY_PATH
```
Please note that the current solution is of `pre-production` quality.<br>


## 3.1 Run with iVSR SDK sample
`vsr_sample` has been developed using the iVSR SDK and OpenCV. For guidance on how to run inference with it, please refer to this [section](./ivsr_sdk/README.md#vsr-sample).<br>

## 3.2 Run with FFmpeg
Once the FFmpeg plugin patches have been applied and FFmpeg has been built, you can refer to [the FFmpeg cmd line samples](ivsr_ffmpeg_plugin/README.md#how-to-run-inference-with-ffmpeg-plugin) for instructions on how to run inference with FFmpeg.<br>

# 4. Model files
Only models in OpenVINO IR format is supported by iVSR. Please reach out to your Intel representative to obtain the model files which are not included in the package.<br>


# 5. License
Please check the license file under each folder.

