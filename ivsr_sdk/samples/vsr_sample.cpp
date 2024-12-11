/********************************************************************************
* INTEL CONFIDENTIAL
* Copyright (C) 2023 Intel Corporation
*
* This software and the related documents are Intel copyrighted materials,
* and your use of them is governed by the express license under
* which they were provided to you ("License").Unless the License
* provides otherwise, you may not use, modify, copy, publish, distribute, disclose or
* transmit this software or the related documents without Intel's prior written permission.
*
* This software and the related documents are provided as is,
* with no express or implied warranties, other than those that are expressly stated in the License.
*******************************************************************************/
#include<string>
#include<dirent.h>
#include<iostream>
#include<unistd.h>
#include "opencv2/opencv.hpp"
#include <opencv2/core/utility.hpp>
#include "ivsr.h"
#include "utils.hpp"
#include "stdlib.h"
#include <chrono>
#include <ctime>
#include <mutex>
#include <list>

typedef std::chrono::high_resolution_clock Time;
typedef std::chrono::nanoseconds ns;


const std::string keys =
    "{h                 | | Print Help Message}"
    "{model_path        | | Required. Path to an openvino ir model(.xml).}"
    "{data_path         | | Required. Path to a folder with images for inference.}"
    "{extension         | | Optional option. Required for CPU custom layers. Absolute path to a shared library with the kernels implementations. }"
    "{device            |CPU| Optional. Specify a target device to infer on, default: CPU. Use MULTI:<comma-separated_devices_list> format to specify MULTI device}"
    "{save_predictions  |false| Optional. Whether to save the infer results to <save_path> or not.}"
    "{save_path         |./outputs| Optional. Path to a folder to save predictions.}"
    "{cldnn_config      | | Optional option. Required for GPU custom kernels. Absolute path to an .xml file with the kernels description.}"
    "{nig               |1| Optional. Number of input data groups for inference. }"
    "{normalize_factor  |255.0| Optional. Normalization factor is equal to the value range required by models, default is 255.0.}"
    "{scale_factor      |2| Optional. The ratio of the size of the image before scaling (original size) to the size of the image after scaling (new size).}"
    "{precision         |f32| Optional. For inference precision.fp32:f32, fp16:f16, bf16:bf16}"
    "{reshape_values    | | Optional. Reshape network to fit the input image size. e.g. --reshape_values=\"(1,3,720,1280)\"}"
    "{num_infer_req     | | Optional. Number of infer request number.}"
   ;

bool checkPath(const std::string& path){
    if (path.empty() || path.back() == '/') {
        return false;
    }

    DIR *pDir = opendir(path.c_str());
    if (pDir == nullptr)
        return false;

    closedir(pDir);
    return true;
}


int getFileNames(std::string path,std::vector<std::string>& filenames)
{
    DIR *pDir;
    struct dirent* ptr;
    if(!(pDir = opendir(path.c_str())))
        return -1;
    while((ptr = readdir(pDir))!=0) {
        if (strcmp(ptr->d_name, ".") != 0 && strcmp(ptr->d_name, "..") != 0)
            filenames.push_back(ptr->d_name);
    }
    sort(filenames.begin(), filenames.end());
    closedir(pDir);
    return 0;
}

// input:   n images with BGR(RGB) 0-255
// output:  NCHW Matrix with RGB(BGR) 0-1
cv::Mat blobFromImages(std::vector<cv::Mat>& images, double scaleFactor,
                   bool swapRB)
{
    for (size_t i = 0; i < images.size(); i++)
        if(images[i].depth() == CV_8U)
            images[i].convertTo(images[i], CV_32F, scaleFactor);

    size_t nimages = images.size();
    cv::Mat image0 = images[0];
    int nch = image0.channels();
    int sz[] = { (int)nimages, nch, image0.rows, image0.cols };
    cv::Mat blob;
    blob.create(4, sz, CV_32F);
    cv::Mat ch[3];

    for(size_t i = 0; i < nimages; i++ )
    {
        const cv::Mat& image = images[i];
        CV_Assert(image.depth() == CV_32F);
        nch = image.channels();
        CV_Assert(nch == 3);
        CV_Assert(image.size() == image0.size());

        for( int j = 0; j < nch; j++ )
            ch[j] = cv::Mat(image.rows, image.cols, CV_32F, blob.ptr((int)i, j));
        if(swapRB)
            std::swap(ch[0], ch[2]);
        cv::split(image, ch);
    }

    return blob;
}

void imagesFromBlob(const cv::Mat& blob_, std::vector<cv::Mat>& images_)
{
    // A blob is a 4 dimensional matrix in floating point precision
    // blob_[0] = batchSize = nbOfImages
    // blob_[1] = nbOfChannels
    // blob_[2] = height
    // blob_[3] = width
    // output : BGR HWC3 image lists
    CV_Assert(blob_.depth() == CV_8U);
    CV_Assert(blob_.dims == 4);
    CV_Assert(blob_.size[1] == 3);

    int sz[] = {blob_.size[2], blob_.size[3]};
    cv::Mat vectorOfChannels[3];
    for (int n = 0; n < blob_.size[0]; ++n)
    {
        for (int c = 0; c < 3; ++c)
        {
            cv::Mat curChannel(2, sz, CV_8U, (void *)blob_.ptr<uchar>(n, c));
            vectorOfChannels[2 - c] = curChannel;   // BGR
        }
        cv::Mat curImage;
        cv::merge(vectorOfChannels, 3, curImage);
        images_.push_back(curImage);
    }
}

std::mutex _mutex;
size_t count = 0;
struct callback_args{
    int flag;
    int nif;
    Time::time_point time;
    callback_args():flag(0), nif(0){}
    callback_args(int f, int nif, Time::time_point t):flag(f),nif(nif),time(t){}
};


void completion_callback (void *args)
{
#ifdef ENABLE_LOG
    std::cout << "[Trace]: "<< "vsr_sample callback called." << std::endl;
#endif
    callback_args * cb_args = (callback_args*)args;
    cb_args->flag = 1;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        count += 1;
    }
#ifdef ENABLE_PERF
    auto duration = get_duration_ms_till_now(cb_args->time);
    std::cout << "Finished group id: " << (count - 1) <<std::endl;
    std::cout << "[PERF] " << "Process Latency: " << double_to_string(duration) <<"ms"<<std::endl;
    std::cout << "[PERF] " << "Process Throughput: " << double_to_string(cb_args->nif * 1000.0 / duration) <<"FPS"<<std::endl;
#endif
}

bool commandLineCheck(int argc, char**argv,std::string keys){
    for(int i = 1; i < argc; i++){
        std::string arg(argv[i]);
        int start_pos = arg.find("--");
        int end_pos = arg.find("=");
        if(start_pos ==-1){
            return false;
        }
        else if(end_pos == -1){
            end_pos = arg.length();
        }
        std::string commandArg = arg.substr(start_pos+2,end_pos - start_pos -2);
        auto pos = keys.find(commandArg,0);
        if(pos >= keys.length()){
            std::cout<<"Error command parameter: --" << commandArg <<std::endl;
            return false;
        }
    }
    return true;
}

void print_tensor_desc(const tensor_desc_t* tensor) {
    if (!tensor) {
        printf("Invalid tensor descriptor!\n");
        return;
    }

    printf("Tensor Descriptor:\n");
    printf("Precision: %s\n", tensor->precision);
    printf("Layout: %s\n", tensor->layout);
    printf("Tensor Color Format: %s\n", tensor->tensor_color_format);
    printf("Model Color Format: %s\n", tensor->model_color_format);
    printf("Scale: %.2f\n", tensor->scale);
    printf("Dimension: %u\n", tensor->dimension);
    printf("Shape: [");

    for (uint8_t i = 0; i < tensor->dimension; i++) {
        printf("%zu", tensor->shape[i]);
        if (i < tensor->dimension - 1) {
            printf(", ");
        }
    }

    printf("]\n");
}

using IVSRFunction = std::function<IVSRStatus(ivsr_handle, char*, char*, ivsr_cb_t*)>;

int main(int argc, char** argv){
    // -------- Parsing and validation of input arguments --------
    cv::CommandLineParser parser(argc, argv, keys);
    if (parser.has("h") || argc == 1)
    {
        parser.printMessage();
        return 0;
    }
    std::string data_path = parser.get<std::string>("data_path");
    int nig = parser.get<int>("nig");
    // int nif = parser.get<int>("nif");
    std::string help = parser.get<std::string>("h");
    bool save_predictions = parser.get<bool>("save_predictions");
    std::string save_path = parser.get<std::string>("save_path");
    float normalize_factor = parser.get<float>("normalize_factor");
    const float NORMALFACTOR_MIN = 1.0;
    const float NORMALFACTOR_MAX = 65535.0;
    if(normalize_factor < NORMALFACTOR_MIN || normalize_factor > NORMALFACTOR_MAX)
    {
        std::cout << "Invalid normalize_factor value! Please enter a value between 1.0 and 255.0."<<std::endl;
        return -1;
    }

    // vsr scale
    int scaleFactor = parser.get<int>("scale_factor");

    if (!parser.check() || !commandLineCheck(argc,argv,keys))
    {
        std::cout << "Error in commandline!" << std::endl;
        parser.printErrors();
        parser.printMessage();
        return -1;
    }
    // input and output path check
    if(!checkPath(data_path)){
        std::cout << "Invalid input directory path!" << std::endl;
        std::cout << "Directory NOT found or PATH ended with '/' " << std::endl;
        std::cout << "Please confirm your path is existed and does NOT end with '/' " << std::endl;
        return -1;
    }
    if(save_predictions && !checkPath(save_path)){
        std::cout << "Invalid output directory path!" << std::endl;
        std::cout << "Directory NOT found or PATH ended with '/' " << std::endl;
        std::cout << "Please confirm your path is existed and does NOT end with '/' " << std::endl;
        return -1;
    }

    // 0. Check inputs and retrieve input frame resolution
    std::cout << "[INFO] " << "Inputs Path: " << data_path << std::endl;
    std::vector<std::string> filePathList;
    getFileNames(data_path, filePathList);
    if (filePathList.size() == 0) {
        std::cout << "No file in input directory path!" << std::endl;
        return -1;
    }
    std::string imgPath = data_path + "/" + filePathList[0];
    cv::Mat img = cv::imread(imgPath, cv::IMREAD_COLOR);
    unsigned int frameHeight = 0, frameWidth = 0;
    frameHeight = img.rows;
    frameWidth  = img.cols;

    // 1. set ivsr config
    std::list<ivsr_config_t *> configs;
    auto add_config = [&configs](IVSRConfigKey key, const void *value) {
        auto new_config = new ivsr_config_t();
        new_config->key = key;
        new_config->value = value;
        new_config->next = nullptr;
        if (!configs.empty())
            (*configs.rbegin())->next = new_config;
        configs.emplace_back(new_config);
    };

    auto model_path = parser.get<std::string>("model_path");
    if (!model_path.empty()) add_config(IVSRConfigKey::INPUT_MODEL, model_path.c_str());

    auto device = parser.get<std::string>("device");
    if (!device.empty()) add_config(IVSRConfigKey::TARGET_DEVICE, device.c_str());

    auto precision = parser.get<std::string>("precision");
    if (!precision.empty()) add_config(IVSRConfigKey::PRECISION, precision.c_str());

    auto extension = parser.get<std::string>("extension");
    if (!extension.empty()) add_config(IVSRConfigKey::CUSTOM_LIB, extension.c_str());

    auto cldnn_config = parser.get<std::string>("cldnn_config");
    if (!cldnn_config.empty()) add_config(IVSRConfigKey::CLDNN_CONFIG, cldnn_config.c_str());

    auto reshape_settings = parser.get<std::string>("reshape_values");
    if (!reshape_settings.empty()) add_config(IVSRConfigKey::RESHAPE_SETTINGS, reshape_settings.c_str());

    auto nireq = parser.get<std::string>("num_infer_req");
    if (!nireq.empty()) add_config(IVSRConfigKey::INFER_REQ_NUMBER, nireq.c_str());

    // in format "<width>,<height>"
    std::string input_res = std::to_string(frameWidth) + "," + std::to_string(frameHeight);
    add_config(IVSRConfigKey::INPUT_RES, input_res.c_str());

    uint8_t dimension_set = 4;
    std::string model_path_lower = model_path;
    std::transform(model_path_lower.begin(), model_path_lower.end(), model_path_lower.begin(), ::tolower);
    // basicvsr has 5 dimensions
    if (model_path_lower.find("basicvsr") != std::string::npos) {
        std::cout << "\"basicvsr\" is found in model_path." << std::endl;
        dimension_set = 5;
    }
 
    tensor_desc_t input_tensor_desc_set = {.precision = "u8",
                                           .layout = "NHWC",
                                           .tensor_color_format = "BGR",
                                           .model_color_format = "RGB",
                                           .scale = normalize_factor,
                                           .dimension = dimension_set,
                                           .shape = {0, 0, 0, 0}};
    tensor_desc_t output_tensor_desc_set = {.precision = "fp32",
                                            .layout = "NCHW",
                                            .tensor_color_format = {0},
                                            .model_color_format = {0},
                                            .scale = 0.0,
                                            .dimension = dimension_set,
                                            .shape = {0, 0, 0, 0}};

    add_config(IVSRConfigKey::INPUT_TENSOR_DESC_SETTING, &input_tensor_desc_set);
    add_config(IVSRConfigKey::OUTPUT_TENSOR_DESC_SETTING, &output_tensor_desc_set);

    // 2. initialize ivsr
    ivsr_handle handle = nullptr;
    auto res = ivsr_init(*configs.begin(), &handle);
    if (res < 0) {
        std::cout <<"Failed to initialize ivsr engine!" <<std::endl;
        return -1;
    }

    tensor_desc_t input_tensor_desc_get = {0}, output_tensor_desc_get = {0};
    ivsr_get_attr(handle, INPUT_TENSOR_DESC, &input_tensor_desc_get);
    ivsr_get_attr(handle, OUTPUT_TENSOR_DESC, &output_tensor_desc_get);
    print_tensor_desc(&input_tensor_desc_get);
    print_tensor_desc(&output_tensor_desc_get);

    int nif = 0;
    res = ivsr_get_attr(handle, IVSRAttrKey::NUM_INPUT_FRAMES, &nif);
    if(res < 0){
        std::cout <<"Failed to get the attribute: " << IVSRAttrKey::NUM_INPUT_FRAMES <<std::endl;
        return -1;
    }
    // 3. prepare input data
    // 3.1 Check the input frames
    if(filePathList.size() < (size_t)nif) {
        std::cout << "Not enough frames for an inference!" << std::endl;
        std::cout << "At least NIF frames in the data_path are needed!" << std::endl;
        return -1;
    }
    std::vector<cv::Mat> inputDataList;
    std::vector<cv::Mat> outputDataList;
    int oriHeight =0,oriWidth=0;
    for(size_t idx =0; idx < filePathList.size() && idx < (size_t)nig * nif; idx += nif){
        // 3.2 Read input images and preprocess with opencv
        std::vector<cv::Mat> inMatList;
        for(int i = 0; i < nif; i++){
            std::string filePath = data_path + "/" + filePathList[idx + i];
#ifdef ENABLE_LOG
            std::cout << "Reading image: " << filePath << std::endl;
#endif
            cv::Mat img = cv::imread(filePath, cv::IMREAD_COLOR);
            inMatList.push_back(img);
        }

        // check images' shape
        oriHeight = inMatList[0].rows, oriWidth = inMatList[0].cols;
        /* how to check image size and model input? */

        // refer to cv::dnn::blobFromImages
        cv::Mat inputNHWC = inMatList[0]; //blobFromImages(inMatList, normalize_factor/255.0, true);
        int sz[] = { 1, nif, oriHeight, oriWidth, 3 };
        cv::Mat inputImg(5, sz, CV_8U, (char *)inputNHWC.data);
        inputDataList.push_back(inputImg.clone());

        int outputSize[] = {1, nif, 3, oriHeight * scaleFactor, oriWidth * scaleFactor};
        cv::Mat outputImg = cv::Mat::zeros(5, outputSize, CV_32F);
        outputDataList.push_back(outputImg.clone());
    }

    std::cout << "[INFO] " << "Inference start: " << std::endl;

    size_t id = 0;
#ifdef ENABLE_PERF
    auto totalStartTime = Time::now();
#endif

    IVSRFunction process_fn = nireq.empty() ? ivsr_process : ivsr_process_async;
    for (; id < inputDataList.size() && id < outputDataList.size(); id++) {
        auto inputImg = inputDataList[id];
        auto outputImg = outputDataList[id];
        ivsr_cb_t cb;
        auto startTime = Time::now();
        callback_args cb_args(0, nif, startTime);
        cb.ivsr_cb = completion_callback;
        cb.args = (void*)(&cb_args);
        // 4. inference
        IVSRStatus result = process_fn(handle, (char*)inputImg.data, (char*)outputImg.data, &cb);
        if (result < 0) {
            std::cout << "Failed to process the inference on input data seq." << id << std::endl;
        }
    }

    //wait for the completion callback
    while (count != id) {
        usleep(100);
    }
#ifdef ENABLE_PERF
    auto duration = get_duration_ms_till_now(totalStartTime);
    std::cout << "[PERF] " << "Total processed groups: " << nig <<std::endl;
    std::cout << "[PERF] " << "Total latency for all input groups: " << double_to_string(duration) <<"ms"<<std::endl;
    std::cout << "[PERF] " << "Average throughput for all input groups: " << double_to_string(nig * nif * 1000.0 / duration) <<"FPS"<<std::endl;
#endif
    std::cout << "[INFO] " << "Inference finished" << std::endl;

    // save outputs
    if (save_predictions) {
        if (save_path == "") {
            std::cout << "save_path is needed!" << std::endl;
            return -1;
        }
        std::cout << "[INFO] "
                  << " Saving outputs to path: " << save_path << std::endl;
        for (size_t idx = 0; idx < outputDataList.size(); idx++) {
            // 5. post process and save outputs
            auto outputImg = outputDataList[idx];
            outputImg.convertTo(outputImg, CV_8U, normalize_factor);
            int nchwSize[] = {nif, 3, oriHeight * scaleFactor, oriWidth * scaleFactor};
            cv::Mat outputNCHW(4, nchwSize, CV_8U, (void*)outputImg.data);
            std::vector<cv::Mat> outMatList;
            imagesFromBlob(outputNCHW, outMatList);

            // save group
            for (int i = 0; i < nif; i++) {
                std::string filePath = save_path + "/" + filePathList[idx * nif + i];
#ifdef ENABLE_LOG
                std::cout << "[Trace]: "
                          << "Saving image: " << filePath << std::endl;
#endif
                cv::imwrite(filePath, outMatList[i]);
            }
        }
    }

    //release resources
    res = ivsr_deinit(handle);
    if(res < 0){
        std::cout <<"Failed to release resources!" <<std::endl;
        return -1;
    }

    for (auto config : configs)
        delete config;
    configs.clear();

    return 0;
}
