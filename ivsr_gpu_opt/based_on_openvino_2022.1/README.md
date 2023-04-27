# iVSR BasicVSR Sample

This folder enables BasicVSR inference on Intel CPU/GPU using OpenVINO as backends.

## Prerequisites:
- Intel Xeon hardware platform. Intel® Data Center GPU Flex Series is optional (Validated on Flex 170, also known as ATS-M1 150W)
- Host OS: Linux based OS (Tested and validated on Ubuntu 20.04)
- kernel: 5.10.54 (Optional to enable Intel® Data Center GPU Flex Series accelerator)
- cmake: 3.16.3
- make: 4.2.1
- git: 2.25.1
- gcc: 9.4.0

If they are not available on your system, please install them.

## Set up BasicVSR OpenVINO Environment
You can quickly set up the OpenVINO environment for BasicVSR inference by `build.sh`. Below is a command line example:
```bash
./build.sh <none or Release or Debug >
```

## BasicVSR Inference Sample
There is a C++ sample to perform BasicVSR inference on OpenVINO backend. You can reach the sample code and the executable file after you  set up the envirnonment successfully. 

You can run `sh <PATH_TO_OPENVINO_PROJECT>/bin/intel64/Release/basicvsr_sample -h` to get help messages and see the default settings of parameters. 

|Option name|Desciption|Default value|Recommended value(s)|
|:--|:--|:--|:--|
|h|Print Help Message|||
|cldnn_config|Optional. Path of CLDNN config for Intel GPU.|None|< Path to OpenVINO >/flow_warp_custom_op/flow_warp.xml|
|data_path|Need. Input data path for inference.|None||
|device|Optional. Device to perform inference.|CPU|CPU or GPU|
|extension|Optional. Extension (.so or .dll) path of custom operation.|None|< Path to basicvsr_sample>/lib/libcustom_extension.so|
|model_path|Need. Path of BasicVSR OpenVINO IR model(.xml).|None||
|nif|Need. Number of input frames for each inference.|3|3|
|patch_evalution|Optional. Whether to crop the original frames to smaller patches for evaluation.|false|false|
|save_path|Optional. Path to save predictions.|./outputs||
|save_predictions|Optional. Whether to save the results to save_path.|true||

Please note that all the pathes specified options should exist and do not end up with '/'. Below is an example to run BasicVSR inference:
```bash
# Run the inference evaluation on CPU
<PATH_TO_OPENVINO_PROJECT>/bin/intel64/basicvsr_sample --model_path=<IR model path(.xml)> --extension=<PATH_TO_OPENVINO_PROJECT>/bin/intel64/lib/libcustom_extension.so --data_path=<Directory path including input frames> --nif=<Number of input frames> --device=CPU --save_predictions --save_path=<Directory path to save results> --cldnn_config=<PATH_TO_OPENVINO_PROJECT>/flow_warp_custom_op/flow_warp.xml
```



