# iVSR FFmpeg plugin
The folder `ivsr_ffmpeg_plugin` enables to do BasicVSR inference using FFmpeg with OpenVINO as backend. The inference is validated with OpenVINO 2022.1 and FFmpeg 5.1. We've implemented some patches on the OpenVINO and FFmpeg to enable the pipeline.
The folder `patches` includes our patches for FFmpeg.

## Prerequisites
The FFmpeg plugin is validated on:
- Intel Xeon hardware platform
- (Optional) Intel Data Center GPU Flex 170(*aka* ATS-M1 150W)
- Host OS: Linux based OS (Ubuntu 20.04)
- Docker OS: Ubuntu 20.04
- kernel: 5.10.54 (Optional to enable Intel® Data Center GPU Flex Series accelerator)
- cmake
- make
- git
- docker 

## Build Docker Image
Before building docker image, make sure you have access to the public repo of OpenVINO and FFmpeg.
The following is the sample command line used for building docker image.
- Set up docker service
```
sudo mkdir -p /etc/systemd/system/docker.service.d
printf "[Service]\nEnvironment=\"HTTPS_PROXY=$https_proxy\" \"NO_PROXY=$no_proxy\"\n" | sudo tee /etc/systemd/system/docker.service.d/proxy.conf
sudo systemctl daemon-reload
sudo systemctl restart docker
```
- Pull the source code of OpenVINO and FFmpeg with git
```
cd <ivsr-local-folder>
git submodule init
git submodule update --remote --recursive
```
- Build docker image
```
cd ivsr_ffmpeg_plugin
sudo docker build -f Dockerfile -t ffmpeg-ov1 ..
```
If the image is built successfully, you can find a docker image named `ffmpeg-ov1:latest` with command `docker images`.

## Start docker container and set up BasicVSR inference environment
When the docker image is built successfully, you can start a container and set up the OpenVINO environment for BasicVSR inference as below. Please note that `--shm-size=128g` is necessary because a large amount of share memory will be requested by the FFmpeg inference filter.
```
docker run -itd --name ffmpeg-ov1 --privileged -e MALLOC_CONF="oversize_threshold:1,background_thread:true,metadata_thp:auto,dirty_decay_ms:9000000000,muzzy_decay_ms:9000000000" --shm-size=128g ffmpeg-ov1-new:latest bash
docker exec -it ffmpeg-ov1 /bin/bash
source /workspace/ivsr/ivsr_gpu_opt/based_on_openvino_2022.1/openvino/install/setupvars.sh
ldconfig
```
The above command lines will start a docker container named `ffmpeg-ov1`.
## How to run BasicVSR inference with FFmpeg-plugin
- Use FFmpeg-plugin to run BasicVSR inference
```
cd /workspace/ivsr/ivsr_ffmpeg_plugin/ffmpeg
./ffmpeg -i <your test video> -vf dnn_processing=dnn_backend=openvino:model=<your model.xml>:input=input:output=output:nif=3:backend_configs='nireq=1&device=CPU' test_out.mp4
```

- Work modes of FFmpeg-plugin for BasicVSR

Decoder and encoder work similar to FFmpeg 5.1. Options and command line formats are not changed.
Only the options of video filter `dnn_processing` are introduced.

|AVOption name|Description|Default value|Recommended value(s)|
|:--|:--|:--|:--|
|dnn_backend|DNN backend framework name|native|openvino|
|input_width|input video width|1920|1920|
|input_height|input video height|1080|1080|
|model|path to model file|NULL|Available full path of the released model files|
|input|input name of the model|NULL|input|
|output|output name of the model|NULL|output|
|backend_configs:nireq|number of inference request|2|1 or 2 or 4|
|backend_configs:device|device for inference task|CPU|CPU or GPU|

Apart from the common AVOptions which can be set in the normal FFmpeg command line with format `AVOption=value`, there are two options `nireq` and `device` to be set with the `backend_configs` option. The command format is `backend_configs='nireq=value&device=value'` when you want to set both of them.

- Inference model precision and custom operation supportive 

OpenVINO models with different precisions can be supported by the FFmpeg-plugin. In this release, we have validated FP32 and INT8 models which are converted and quantized by the model optimizer of OpenVINO 2022.1.

The [Custom OpenVINO™ Operations](https://docs.openvino.ai/latest/openvino_docs_Extensibility_UG_add_openvino_ops.html) is a great feature of OpenVINO which allows users to support models with operations that OpenVINO does not support out-of-the-box. 
This FFmpeg-plugin supports BasicVSR which utilizes custom operations. There is no additional options required for the command line in the case you have set the right path of the model. Please be noted that the dependent files are located in the docker container folder `/workspace/ivsr/ivsr_gpu_opt/based_on_openvino_2022.1/openvino/flow_warp_custom_op` and `/workspace/ivsr/ivsr_gpu_opt/based_on_openvino_2022.1/openvino/bin/intel64/Release/lib`, do not make any changes to them.

## (Optional) Intel® Data Center GPU Flex Series Supportive
The default docker image doesn't include Intel® Data Center GPU Flex Series driver, so you may not be able to accelerate inference with GPU in the FFmpeg pipeline. You can start a docker container by the above steps and install the driver in the docker container.

Below is the selected components during the driver installation. Some indications may be different, but you can reference this component selection.
```
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
```
