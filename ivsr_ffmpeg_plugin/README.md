# iVSR FFmpeg plugin
This folder enables to do BasicVSR inference using FFmpeg with OpenVINO as backend. The inference backend is OpenVINO 2022.1, and the FFmpge version is 5.1. We've implemented some patches on the OpenVINO 2022.1 and FFmpeg 5.1 to make to pipeline able to work.
The patches for FFmpeg are located in folder 'patches'.

## Prerequisites:
Hardware: Intel Xeon hardware platform. Intel® Data Center GPU Flex Serie is optional (Validated on Flex 170)

OS: The FFmpeg plugin is validated on
- Host OS: Linux based OS (Tested and validated on Ubuntu 20.04)
- Docker OS: Ubuntu 20.04
- kernel: 5.10.54 (Optional to enable Intel® Data Center GPU Flex Series accelerator)

Build Tools: Install cmake, make, git and docker if they are not available on your system.

## Build Docker Image:
Before building docker image, you need to have installed git and docker on your system, and make sure your have access to the ivsr, openvino and ffmpeg public repo to pull their source code.
Below is the sample command line which is used for our validation.
```
sudo mkdir -p /etc/systemd/system/docker.service.d
printf "[Service]\nEnvironment=\"HTTPS_PROXY=$https_proxy\" \"NO_PROXY=$no_proxy\"\n" | sudo tee /etc/systemd/system/docker.service.d/proxy.conf
sudo systemctl daemon-reload
sudo systemctl restart docker
cd <ivsr-local-folder>
git submodule init
git submodule update --remote --recursive
cd ivsr_ffmpeg_plugin
sudo docker build -f Dockerfile -t ffmpeg-ov1 ..
```
If the docker building process is successful, you can find a docker image name '*ffmpeg-ov1:latest*' with command '*docker images*'.

## Start docker container and set up BasicVSR OpenVINO environment
When the docker image is built successfully, you can start a container and set up the OpenVINO environment as below. Please be noted the '*--shm-size=128g*' is necessary because big amount of share memory will be requested from the ffmpeg inference filter.

    docker run -itd --name ffmpeg-ov1 --privileged -e MALLOC_CONF="oversize_threshold:1,background_thread:true,metadata_thp:auto,dirty_decay_ms:9000000000,muzzy_decay_ms:9000000000" --shm-size=128g ffmpeg-ov1-new:latest bash
    docker exec -it ffmpeg-ov1 /bin/bash
    source /workspace/ivsr/ivsr_gpu_opt/based_on_openvino_2022.1/openvino/install/setupvars.sh
	ldconfig
The above command lines will start a docker container named '*ffmpeg-ov1*'

## Use FFmpeg-plugin to run pipeline with BasicVSR
    cd /workspace/ivsr/ivsr_ffmpeg_plugin/ffmpeg
    ./ffmpeg -i <your test video> -vf dnn_processing=dnn_backend=openvino:model=<your model.xml>:input=input:output=output:nif=3:backend_configs='nireq=1&device=CPU' test_out.mp4

## Workmodes of FFmpeg-plugin for BasicVSR
The FFmpeg decoder and encoder workmodes are similiar to its based 5.1 version. Options for decoder and encoder, including command line format, are no changed.
Only the options of video filter 'dnn_processing' are introduced here.

|AVOption name|Desciption|Default value|Recommended value(s)|
|:--|:--|:--|:--|
|dnn_backend|DNN backend framework name|native|openvino|
|input_width|input video width|1920|1920|
|input_height|input video height|1080|1080|
|model|path to model file|NULL|Available full path of the released model files|
|input|input name of the model|NULL|input|
|output|output name of the model|NULL|output|
|backend_configs:nireq|number of inference request|2|1,2,4|
|backend_configs:device|Device for inference task|CPU|CPU,GPU|

Besides the common AVOptions which can be set in the normal FFmpeg command line with *AVOption=value* format, there are two options *nireq* and *device* to be set with the *backend_configs* option. The command format is *backend_configs='nireq=value&device=value'* when you want to set both of them.

## Inference model precision and custom operation supportive
There are different precisions of OpenVINO model which can all be supported with the FFmpeg-plugin dependended OpenVINO Runtime library. In this release, we have validated FP32 and INT8 models which are converted and quantized by the model optimizer from OpenVINO 2022.1.
The custom operation is a great feature of OpenVINO which allows user to register custom operations to support models with operations which OpenVINO™ does not support out-of-the-box. This FFmpeg-plugin can support a BasicVSR model which utilizes custom operation. There is no addtional option required for the command line in the case you have set the right path of the model which has custom op. Please be noted the dependent files are located in the docker container folder '*/workspace/ivsr/ivsr_gpu_opt/based_on_openvino_2022.1/openvino/flow_warp_custom_op*' and '*/workspace/ivsr/ivsr_gpu_opt/based_on_openvino_2022.1/openvino/bin/intel64/Release/lib*', and don't make any changes for these two folder.

## Optional: Intel® Data Center GPU Flex Serie Supportive
The default docker image doesn't include Intel® Data Center GPU Flex Serie driver installed, so you are not able to accelerate inference with GPU in the ffmpeg pipeline. You may start a docker container by the above steps, and install the driver by your own in the docker container.
Below is the selected component during our installation. Some indications may be different but you can reference this component selection.

    Do you want to update kernel xx.xx.xx ?
	'y/n' default is y:n
	Do you want to install mesa ?
	'y/n' default is y:n
	Do you want to install media ?
	'y/n' default is y:y
	Do you want to install opencl ?
	'y/n' default is n:y
	Do you want to install level zero ?
	'y/n' default is n:n
	Do you want to install ffmpeg ?
	'y/n' default is n:n
	Do you want to install tools ?
	'y/n' default is n:n
