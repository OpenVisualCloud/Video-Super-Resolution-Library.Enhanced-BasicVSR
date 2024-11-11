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
#include<cassert>
#include<string>
#include<unordered_map>
#include<map>
#include "ivsr.h"
#include "engine.hpp"
#include "ov_engine.hpp"
#include "InferTask.hpp"
#include "ivsr_smart_patch.hpp"
#include "threading/ivsr_thread_executor.hpp"
#include "utils.hpp"
#include <mutex>
#include <sstream>
#include <cctype>
#include <algorithm>

std::vector<std::string> parse_devices(const std::string& device_string) {
    std::string comma_separated_devices = device_string;
    auto colon = comma_separated_devices.find(":");
    std::vector<std::string> result;
    if (colon != std::string::npos) {
        auto target_device = comma_separated_devices.substr(0, colon);
        if (target_device == "AUTO" || target_device == "MULTI") {
            result.push_back(target_device);
        }
        auto bracket = comma_separated_devices.find("(");  // e.g. in BATCH:GPU(4)
        comma_separated_devices = comma_separated_devices.substr(colon + 1, bracket - colon - 1);
    }

    auto devices = split(comma_separated_devices, ',');
    result.insert(result.end(), devices.begin(), devices.end());
    return result;
}

void parse_engine_config(std::map<std::string, ov::AnyMap> &config, std::string device, std::string infer_precision, std::string cldnn_config){
    auto getDeviceTypeFromName = [](std::string device) -> std::string {
        return device.substr(0, device.find_first_of(".("));
    };
    if(device.find("GPU") != std::string::npos && !cldnn_config.empty()){
        if (!config.count("GPU"))
                config["GPU"] = {};
        config["GPU"]["CONFIG_FILE"] = cldnn_config;
    }

    auto devices = parse_devices(device);
    std::set<std::string> default_devices;
    for (auto& d : devices) {
        auto default_config = config.find(getDeviceTypeFromName(d));
        if (default_config != config.end()) {
            if (!config.count(d)) {
                config[d] = default_config->second;
                default_devices.emplace(default_config->first);
            }
        }
    }
    for (auto& d : default_devices) {
        config.erase(d);
    }
    // check if using the virtual device
    auto if_multi = std::find(devices.begin(), devices.end(), "MULTI") != devices.end();
    // remove the hardware devices if MULTI appears in the devices list.
    auto hardware_devices = devices;
    if (if_multi) {
	ivsr_version_t version;
        ov::Version ov_version = ov::get_openvino_version();
	std::string ov_buildNumber = std::string(ov_version.buildNumber);
        // Parse out the currect virtual device as the target device.
        std::string virtual_device = split(device, ':').at(0);
        auto iter_virtual = std::find(hardware_devices.begin(), hardware_devices.end(), virtual_device);
        hardware_devices.erase(iter_virtual);
	if (ov_buildNumber.find("2022.3") != std::string::npos) {
	    devices.clear();
            devices.push_back(virtual_device);
	} else {
	    devices = hardware_devices;
	}
    }
    // update config per device
    int nstream = 1; // set nstream = 1 for GPU what about CPU?
    //std::string infer_precision = "f32"; // set infer precision to f32
    //std::string infer_precision = "f16"; // set infer precision to f32
    for (auto& d : devices) {
        auto& device_config = config[d];
        try {
            // set throughput streams and infer precision for hardwares
            if (d == "MULTI" || d == "AUTO") {
                for(auto& hd : hardware_devices){
                    // construct device_config[hd] map and insert first property
                    device_config.insert(ov::device::properties(hd, ov::num_streams(nstream)));
                    // insert second property in device_config[hd]
                    auto& property = device_config[hd].as<ov::AnyMap>();
                    property.emplace(ov::hint::inference_precision(infer_precision));
                }
            }
            else if(d.find("GPU") != std::string::npos){ // GPU
                device_config.emplace(ov::num_streams(nstream));
                device_config.emplace(ov::hint::inference_precision(infer_precision));
            }
            else{ // CPU
                // insert inference precision to map device_config
                device_config.emplace(ov::hint::inference_precision(infer_precision));
            }
        } catch (const ov::AssertFailure& e) {
            std::cerr << "Caught an ov::AssertFailure exception: " << e.what() << std::endl;
            return;
        } catch (...) {
            std::cerr << "Caught an unknown exception" << std::endl;
            return;
        }
    }
}

bool check_engine_config(std::string model, std::string device) {
    if(model == "") {
        ivsr_status_log(IVSRStatus::UNSUPPORTED_CONFIG,"please set model!");
        return false;
    }
    if(device == "") {
        ivsr_status_log(IVSRStatus::UNSUPPORTED_CONFIG,"please set device!");
        return false;
    }
    return true;
}

// Helper function to trim left and right whitespace from a string.
void trim(std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        start++;
    }

    auto end = str.rbegin();
    while (end.base() != start && std::isspace(*end)) {
        end++;
    }

    str.assign(start, end.base());
}

// Converts a string containing comma-separated values into a vector of size_t.
std::vector<size_t> convert_string_to_vector(const std::string& input) {
    std::vector<size_t> result;

    // Remove parenthesis from the input and trim whitespace.
    size_t first = input.find_first_of('(');
    size_t last = input.find_last_of(')');
    std::string data =
        (first != std::string::npos && last != std::string::npos) ? input.substr(first + 1, last - first - 1) : input;

    trim(data);  // Trim the string to remove leading and trailing spaces.

    std::istringstream iss(data);
    std::string token;

    while (std::getline(iss, token, ',')) {
        trim(token);  // Trim each token before conversion.

        // Check if the string is a number.
        bool isNumber = !token.empty() && std::all_of(token.begin(), token.end(), ::isdigit);

        if (isNumber) {
            try {
                size_t value = std::stoull(token);
                result.push_back(value);
            } catch (const std::out_of_range&) {
                // Handle error if the number is too large for size_t
                std::cerr << "Out of range: " << token << std::endl;
            }
        } else {
            // Handle the error for non-numeric tokens.
            std::cerr << "Invalid argument: " << token << std::endl;
        }
    }

    return result;
}

struct ivsr{
    engine<ov_engine> inferEngine;
    IVSRThread::IVSRThreadExecutor *threadExecutor;
    std::unordered_map<std::string,const char*> vsr_config;
    PatchConfig patchConfig;
    bool patchSolution;
    std::vector<size_t> input_data_shape; //shape of input data
    ivsr():threadExecutor(nullptr),patchSolution(false){}
};

IVSRStatus ivsr_init(ivsr_config_t *configs, ivsr_handle *handle){
    if(configs == nullptr || handle == nullptr){
        ivsr_status_log(IVSRStatus::GENERAL_ERROR,"in ivsr_init");
        return IVSRStatus::GENERAL_ERROR;
    }

    (*handle) = new ivsr();

    //TODO: replace w/ parseConfig() ??
    // 1.parse input config
    std::string model = "", device = "", batch = "", infer_precision = "f32";
    std::string verbose = "", custom_lib = "", cldnn_config = "";
    std::vector<size_t> reshape_settings;
    std::vector<size_t> reso;
    size_t frame_width = 0, frame_height = 0;
    while(configs!=nullptr){
        IVSRStatus unsupported_status = IVSRStatus::OK;
        std::string unsupported_output = "";
        switch(configs->key){
            case IVSRConfigKey::INPUT_MODEL:
                model = std::string(configs->value);
                if(!checkFile(model)){
                    unsupported_status = IVSRStatus::UNSUPPORTED_CONFIG;
                    unsupported_output.append("INPUT_MODEL").append("=").append(configs->value);
                    return IVSRStatus::UNSUPPORTED_CONFIG;
                }
                std::cout << "[INFO] " << "Model Path:" << model << std::endl;
                break;
            case IVSRConfigKey::TARGET_DEVICE:
                device = configs->value;
                std::cout << "[INFO] " << "DEVICE:" << device << std::endl;
                break;
            case IVSRConfigKey::BATCH_NUM:
                batch = configs->value;
                break;
            case IVSRConfigKey::VERBOSE_LEVEL:
                verbose = configs->value;
                break;
            case IVSRConfigKey::CUSTOM_LIB:
                custom_lib = configs->value;
                if(!checkFile(custom_lib)){  // file not exists, inform out
                    unsupported_status = IVSRStatus::UNSUPPORTED_CONFIG;
                    unsupported_output.append("CUSTOM_LIB").append("=").append(configs->value);
                    return IVSRStatus::UNSUPPORTED_CONFIG;
                }
                break;
            case IVSRConfigKey::CLDNN_CONFIG:
                cldnn_config = configs->value;
                if(!checkFile(cldnn_config)){
                    unsupported_status = IVSRStatus::UNSUPPORTED_CONFIG;
                    unsupported_output.append("CLDNN_CONFIG").append("=").append(configs->value);
                    return IVSRStatus::UNSUPPORTED_CONFIG;
                }
                break;
            case IVSRConfigKey::PRECISION:
                infer_precision = configs->value;
                if(device.find("GPU") != std::string::npos){
                    if(infer_precision.compare("f32") != 0 && infer_precision.compare("f16") != 0){
                        ivsr_status_log(IVSRStatus::UNSUPPORTED_CONFIG,"for PRECISION=");
                        return IVSRStatus::UNSUPPORTED_CONFIG;
                    }
                }else{
                    if(infer_precision.compare("f32") != 0 && infer_precision.compare("bf16") != 0){
                        ivsr_status_log(IVSRStatus::UNSUPPORTED_CONFIG,"for PRECISION=");
                        return IVSRStatus::UNSUPPORTED_CONFIG;
                    }
                }
                break;
            case IVSRConfigKey::RESHAPE_SETTINGS:
                reshape_settings = convert_string_to_vector(configs->value);
                break;
            case IVSRConfigKey::INPUT_RES:
                //in format "<width>,<height>"
                reso = convert_string_to_vector(configs->value);
                frame_width  = reso[0];
                frame_height = reso[1];
                break;
            default:
                unsupported_status = IVSRStatus::UNSUPPORTED_KEY;
                unsupported_output.append(std::to_string(configs->key));
                break;
        }
        ivsr_status_log(unsupported_status, unsupported_output.c_str());

        configs = configs->next;
    }
    if(!check_engine_config(model, device)) {
        return IVSRStatus::UNSUPPORTED_CONFIG;
    }
    if(frame_width == 0 || frame_height == 0) {
        ivsr_status_log(IVSRStatus::UNSUPPORTED_CONFIG,"please set INPUT_RES!");
        return IVSRStatus::UNSUPPORTED_CONFIG;
    }

    /**
     * Below code only is for OpenVINO engine
    */
    // 2.parse config for inference engine
    std::map<std::string, ov::AnyMap> engine_configs;
    parse_engine_config(engine_configs,device,infer_precision,cldnn_config);

    // 3.construct and initialization
    // - initialize inference engine
    (*handle)->inferEngine = {new ov_engine(device, model, custom_lib, engine_configs, reshape_settings)};
    // -construct IVSRThreadExecutor object
    IVSRThread::Config executorConfig;
    (*handle)->threadExecutor = new IVSRThread::IVSRThreadExecutor(executorConfig, (*handle)->inferEngine.get_impl());

    // -construct patch config
    size_t input_dims = 0;
    (*handle)->inferEngine.get_attr("input_dims", input_dims);
    ov::Shape model_inputs, model_outputs;
    (*handle)->inferEngine.get_attr("model_inputs", model_inputs); // 1,3,3,1080,1920 (400,700)
    (*handle)->inferEngine.get_attr("model_outputs", model_outputs); // 1,3,3,2160,3840 (800,1400)

    // --set patch configs
    int m_input_width = *(model_inputs.end() - 1);
    int m_input_height = *(model_inputs.end() - 2);
    int nif = *(model_inputs.end() - 4);
    int m_output_width = *(model_outputs.end() - 1);
    (*handle)->patchConfig.scale = m_output_width / m_input_width; // Note: do not support fractional SR
    (*handle)->patchConfig.patchHeight = m_input_height;
    (*handle)->patchConfig.patchWidth = m_input_width;
    (*handle)->patchConfig.dims = input_dims;
    (*handle)->patchConfig.nif = nif;
#ifdef ENABLE_LOG
    std::cout << "[Trace]: " << (*handle)->patchConfig << std::endl;
#endif

    // generate input data shape
    std::vector<size_t>& input_shape = (*handle)->input_data_shape;
    //model input res might not be the same as input frame res
    std::transform(model_inputs.begin(), model_inputs.end(), std::back_inserter(input_shape),
                   [](size_t val) { return val; });
    input_shape[input_shape.size() - 1] = frame_width;
    input_shape[input_shape.size() - 2] = frame_height;

    return IVSRStatus::OK;
}

IVSRStatus ivsr_process(ivsr_handle handle, char* input_data, char* output_data, ivsr_cb_t* cb){
    if(input_data == nullptr){
        ivsr_status_log(IVSRStatus::GENERAL_ERROR, "in ivsr_process");
        return IVSRStatus::GENERAL_ERROR;
    }

    try{
        std::vector<int> int_shape;
        std::transform(handle->input_data_shape.begin(), handle->input_data_shape.end(), std::back_inserter(int_shape),
                       [](size_t val) -> int { return static_cast<int>(val); });

        // determine whether apply patch solution or not
        if(handle->patchConfig.patchHeight < *(int_shape.end()-2) ||
           handle->patchConfig.patchWidth  < *(int_shape.end()-1)) {
            handle->patchSolution = true;
        }

        // smart patch inference
        SmartPatch* smartPatch = new SmartPatch(handle->patchConfig, input_data, output_data, int_shape, handle->patchSolution);
        // -prepare data
        auto res = smartPatch->generatePatch();
        if(res == -1){
            delete smartPatch;
            ivsr_status_log(IVSRStatus::UNKNOWN_ERROR, "in Smart Patch");
            return IVSRStatus::UNKNOWN_ERROR;
        }
        auto patchList = smartPatch->getInputPatches();
        auto outputPatchList = smartPatch->getOutputPatches();

#ifdef ENABLE_PERF
    auto totalStartTime = Time::now();
#endif

    // create infer requests based on patch list size
    if (patchList.size() > handle->inferEngine.get_infer_requests_size()) {
        auto res = handle->inferEngine.create_infer_requests(patchList.size());
        if (res == -1) {
            std::cout << "[ERROR]: " << "Failed to creat infer requests!\n";
            delete smartPatch;
            return IVSRStatus::GENERAL_ERROR;
        }
    }

	// -get data into infer task
        int idx = 0;
	for(; idx < patchList.size(); idx++ ){
#ifdef ENABLE_LOG
            std::cout << "[Trace]: " << "ivsr_process on patch: " << idx <<  std::endl;
#endif

	    std::shared_ptr<InferTask> task = handle->threadExecutor->CreateTask(patchList[idx], outputPatchList[idx], InferFlag::AUTO);
        handle->threadExecutor->Enqueue(task);
    }

	// -wait for all the tasks finish
        handle->threadExecutor->wait_all(patchList.size());
#ifdef ENABLE_PERF
        auto duration = get_duration_ms_till_now(totalStartTime);
        std::cout << "[PERF] " << "Patch inference with memory copy - Latency: " << double_to_string(duration) <<"ms"<<std::endl;
        std::cout << "[PERF] " << "Patch inference with memory copy - Throughput: " << double_to_string(handle->patchConfig.nif* 1000.0 / duration) <<"FPS"<<std::endl;
#endif
// #ifdef ENABLE_PERF
// 	// -get total duration for all tasks
// 	      double totalDuration = handle->threadExecutor->get_duration_in_milliseconds();
//         double fps = 3 * 1000.0/totalDuration;

// 	      std::cout << "All tasks Total Latency for One Nig: " << double_to_string(totalDuration) <<"ms"<<std::endl;
//         std::cout << "All tasks in One Nig Throughput : " << double_to_string(fps) <<"FPS"<<std::endl;
// #endif
	// -restore output patches to images
        res = smartPatch->restoreImageFromPatches();
        if(res == -1){
            ivsr_status_log(IVSRStatus::UNKNOWN_ERROR, "in Smart Patch");
            return IVSRStatus::UNKNOWN_ERROR;
        }

        delete smartPatch;
        // notify user
        cb->ivsr_cb(cb->args);

    }catch(exception e){
        std::cout << "Error in ivsr_process" << std::endl;
        ivsr_status_log(IVSRStatus::EXCEPTION_ERROR, e.what());
        return IVSRStatus::UNKNOWN_ERROR;
    }

    return IVSRStatus::OK;
}


IVSRStatus ivsr_reconfig(ivsr_handle handle, ivsr_config_t* configs){
    if(configs == nullptr){
        ivsr_status_log(IVSRStatus::GENERAL_ERROR, "in ivsr_reconfig");
        return IVSRStatus::GENERAL_ERROR;
    }

    try{
        // reset config in container
        while(configs!=nullptr){
            switch(configs->key){
                case IVSRConfigKey::INPUT_MODEL:
                    handle->vsr_config["model"] = configs->value;
                    break;
                case IVSRConfigKey::TARGET_DEVICE:
                    handle->vsr_config["device"] = configs->value;
                    break;
                case IVSRConfigKey::BATCH_NUM:
                    handle->vsr_config["batch_num"] = configs->value;
                    break;
                case IVSRConfigKey::VERBOSE_LEVEL:
                    handle->vsr_config["verbose_level"] = configs->value;
                    break;
                case IVSRConfigKey::CUSTOM_LIB:
                    handle->vsr_config["custom_lib"] = configs->value;
                    break;
                case IVSRConfigKey::CLDNN_CONFIG:
                    handle->vsr_config["cldnn_config"] = configs->value;
                    break;
                default:
                    break;
            }
            configs = configs->next;
        }

        // reconfig ov_engine ?

    }catch(exception e){
        // std::cout << "Error in ivsr_reconfig" << std::endl;
        ivsr_status_log(IVSRStatus::EXCEPTION_ERROR, e.what());
        return IVSRStatus::UNKNOWN_ERROR;
    }

    return IVSRStatus::OK;
}

IVSRStatus ivsr_get_attr(ivsr_handle handle, IVSRAttrKey key, void* value){
    switch (key)
    {
        case IVSRAttrKey::IVSR_VERSION:
        {
            value = (void *)"ivsr_release:2024/05";
            break;
        }
        case IVSRAttrKey::INPUT_TENSOR_DESC:
        {
            int* input_tensor_desc = (int*) value;
            ov::Shape input_shape;
            handle->inferEngine.get_attr("model_inputs", input_shape);
            for(auto s : input_shape){
                *input_tensor_desc = s;
                input_tensor_desc++;
            }
            break;
        }
        case IVSRAttrKey::OUTPUT_TENSOR_DESC:
        {
            int* output_tensor_desc = (int*) value;
            ov::Shape output_shape;
            handle->inferEngine.get_attr("model_outputs", output_shape);
            for(auto s : output_shape){
                *output_tensor_desc = s;
                output_tensor_desc++;
            }
            break;
        }
        case IVSRAttrKey::NUM_INPUT_FRAMES:
        {
            int* nif = (int*) value;
            *nif = handle->patchConfig.nif;
            break;
        }
        case IVSRAttrKey::INPUT_DIMS:
        {
            size_t dims = 0;
            handle->inferEngine.get_attr("input_dims", dims);
            *((size_t *)value) = dims;
            break;
        }
        case IVSRAttrKey::OUTPUT_DIMS:
        {
            size_t dims = 0;
            handle->inferEngine.get_attr("output_dims", dims);
            *((size_t *)value) = dims;
            break;
        }
        default:
        {
            ivsr_status_log(IVSRStatus::UNSUPPORTED_KEY,(char*)key);
            return IVSRStatus::UNSUPPORTED_KEY;
        }
    }
    return IVSRStatus::OK;
}

IVSRStatus ivsr_deinit(ivsr_handle handle) {
    if (handle == nullptr) {
        ivsr_status_log(IVSRStatus::GENERAL_ERROR, "Invalid handle");
        return IVSRStatus::GENERAL_ERROR;
    }

    try {
        auto p = handle->inferEngine.get_impl();
        if (p != nullptr)
            delete p;

        if (handle->threadExecutor) {
            delete handle->threadExecutor;
            handle->threadExecutor = nullptr;
        }
    } catch (exception e) {
        ivsr_status_log(IVSRStatus::EXCEPTION_ERROR, e.what());
        return IVSRStatus::UNKNOWN_ERROR;
    }

    delete handle;
    return IVSRStatus::OK;
}
