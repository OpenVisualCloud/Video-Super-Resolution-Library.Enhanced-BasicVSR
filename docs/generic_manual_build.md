# Generic Manual Build Steps for FFmpeg + IVSR plugin Software

### 1. (Optional) Install software for Intel® Data Center GPU Flex Series
To facilitate inference on Intel Data Center GPU, it's necessary to have both the kernel driver and the run-time driver and software installed. If you're planning to run inference on a CPU only, you can disregard this step.<br>

The detailed installation instruction is on [this page](https://dgpu-docs.intel.com/driver/installation.html#).<br>


### 2. Install OpenCV
OpenCV, which is used by the iVSR SDK sample for image processing tasks, needs to be installed. Detailed installation instructions can be found at [Installation OpenCV in Linux](https://docs.opencv.org/4.x/d7/d9f/tutorial_linux_install.html).<br>

### 3. Install OpenVINO
OpenVINO, currently the only backend supported by iVSR for model inference, should also be installed. You can refer to this [instruction](https://github.com/openvinotoolkit/openvino/blob/master/docs/dev/build_linux.md) to build OpenVINO from the source code.<br>

### 4. Build iVSR SDK
Once the dependencies are installed in the system, you can proceed to build the iVSR SDK and its sample.<br>
```bash
source <OpenVINO installation dir>/install/setupvars.sh
export OpenCV_DIR=<OpenCV installation dir>/install/lib/cmake/opencv4
cd ivsr_sdk
mkdir -p ./build
cd ./build
cmake .. -DENABLE_THREADPROCESS=ON -DENABLE_SAMPLE=ON -DCMAKE_BUILD_TYPE=Release
make
make install
```
### 5. Build FFmpeg with iVSR plugin
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
./configure --enable-libivsr --extra-cflags=-fopenmp --extra-ldflags=-fopenmp
make -j $(nproc --all)
make install
```
