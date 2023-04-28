# iVSR GPU Optimization for OpenVINO 2022.1

This folder `based_on_openvino_2022.1` enables BasicVSR inference on Intel CPU/GPU using OpenVINO 2022.1 as backend.

## Prerequisites:
- Intel Xeon hardware platform 
- (Optional) Intel Data Center GPU Flex 170(*aka* ATS-M1 150W)
- OS: Ubuntu 20.04
- kernel: 5.10.54 
- cmake: 3.16.3
- make: 4.2.1
- git: 2.25.1
- gcc: 9.4.0

## Set up BasicVSR OpenVINO Environment
You can quickly set up the OpenVINO environment for BasicVSR inference by `build.sh`. Below is a command line example:
```bash
./build.sh <none or Release or Debug>
```

## BasicVSR Inference Sample
There is a C++ sample to perform BasicVSR inference on OpenVINO backend. You can reach the sample code and the executable file after you set up the envirnonment successfully. 

You can run `<PATH_TO_OPENVINO_PROJECT>/bin/intel64/Release/basicvsr_sample -h` to get help messages and see the default settings of parameters. 

|Option name|Desciption|Default value|Recommended value(s)|
|:--|:--|:--|:--|
|h|Print Help Message|||
|cldnn_config|Need. Path of CLDNN config.|None|< Path to OpenVINO >/flow_warp_custom_op/flow_warp.xml|
|data_path|Need. Input data path for inference.|None||
|device|Optional. Device to perform inference.|CPU|CPU or GPU|
|extension|Optional. Extension (.so or .dll) path of custom operation.|None|< Path to basicvsr_sample>/lib/libcustom_extension.so|
|model_path|Need. Path of BasicVSR OpenVINO IR model (.xml).|None||
|nif|Need. Number of input frames for each inference.|3|3|
|save_path|Optional. Path to save predictions.|./outputs||
|save_predictions|Optional. Whether to save the results to save_path.||If this option exists, results will be saved.|

Please note that all the pathes specified options should exist and do not end up with '/'. Here are some examples to run BasicVSR inference:
```bash
# Run the inference evaluation on CPU
<PATH_TO_OPENVINO_PROJECT>/bin/intel64/basicvsr_sample -model_path=<IR model path(.xml)> -extension=<PATH_TO_OPENVINO_PROJECT>/bin/intel64/lib/libcustom_extension.so -data_path=<Directory path including input frames> -nif=<Number of input frames> -device=CPU -save_predictions -save_path=<Directory path to save results> -cldnn_config=<PATH_TO_OPENVINO_PROJECT>/flow_warp_custom_op/flow_warp.xml

# Run the inference evaluation on GPU
<PATH_TO_OPENVINO_PROJECT>/bin/intel64/basicvsr_sample -model_path=<IR model path(.xml)> -extension=<PATH_TO_OPENVINO_PROJECT>/bin/intel64/lib/libcustom_extension.so -data_path=<Directory path including input frames> -nif=<Number of input frames> -device=GPU -save_predictions -save_path=<Directory path to save results> -cldnn_config=<PATH_TO_OPENVINO_PROJECT>/flow_warp_custom_op/flow_warp.xml
```



