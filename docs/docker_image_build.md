# Docker image build guide

### 1. Set timezone correctly before building docker image.
The following command takes Shanghai as an example.

  ```bash
  timedatectl set-timezone Asia/Shanghai
  ```

### 2. Set up docker service

```bash
sudo mkdir -p /etc/systemd/system/docker.service.d
printf "[Service]\nEnvironment=\"HTTPS_PROXY=$https_proxy\" \"NO_PROXY=$no_proxy\"\n" | sudo tee  /etc/systemd/system/docker.service.d/proxy.conf
sudo systemctl daemon-reload
sudo systemctl restart docker
```

### 3. Build docker image

```bash
cd ./ivsr_ffmpeg_plugin
./build_docker.sh --enable_ov_patch [true|false] --ov_version [2022.3|2023.2|2024.5|2024.5s] --os_version [rockylinux9|ubuntu22]
```
- `enable_ov_patch`: Set as `true` or `flase` to enable or disable the application of OpenVINO 2022.3 patches, which are needed to support the Enhanced BasicVSR model.<br>
- `ov_version`: Set the OpenVINO version to `2022.3`, `2023.2`, `2024.5`, `2024.5s`, which will be built and installed, the 2024.5s mean install openvino 2024.5 via apt or yum not build and install from source code. iVSR currently supports both OpenVINO 2022.3, 2023.2 and 2024.5, but the patches to enable the Enhanced BasicVSR model are only for OpenVINO 2022.3.<br>
- `os_version`: Set OS version of Docker image to ubuntu22(Ubuntu 22.04) or rockylinux9(Rocky Linux 9.3) to build docker image based on specific OS.<br>
If the docker image builds successfully, you can see a docker image named `ffmpeg_ivsr_sdk_${os_version}_ov${ov_version}` such as `ffmpeg_ivsr_sdk_ubuntu22_ov2022.3` or `ffmpeg_ivsr_sdk_rockylinux9_ov2022.3` in the output of `docker image ls`.<br>

### 4. Start Docker Container

```bash
sudo docker run -itd --name ffmpeg_ivsr_sdk_container --privileged -e MALLOC_CONF="oversize_threshold:1,background_thread:true,metadata_thp:auto,dirty_decay_ms:9000000000,muzzy_decay_ms:9000000000" -e http_proxy=$http_proxy -e https_proxy=$https_proxy -e no_proxy=$no_proxy --shm-size=128g --device=/dev/dri:/dev/dri ffmpeg_ivsr_sdk_[ubuntu22|rockylinux9]_[ov2022.3|ov2023.2|ov2024.5]:latest bash
sudo docker exec -it ffmpeg_ivsr_sdk_container bash
```
Note `--device=/dev/dri:/dev/dri` is specified in the command to add the host gpu device to container.<br>