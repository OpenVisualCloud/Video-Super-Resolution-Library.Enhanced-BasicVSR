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

void parse_engine_config(std::map<std::string, ov::AnyMap>& config,
                         const std::string& device,
                         const std::string& infer_precision,
                         const std::string& cldnn_config) {
    auto getDeviceTypeFromName = [](std::string device) -> std::string {
        return device.substr(0, device.find_first_of(".("));
    };
    if (device.find("GPU") != std::string::npos && !cldnn_config.empty()) {
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
        //ivsr_version_t version;
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
    int nstream = 1;  // set nstream = 1 for GPU what about CPU?
    for (auto& d : devices) {
        auto& device_config = config[d];
        try {
            // set throughput streams and infer precision for hardwares
            if (d == "MULTI" || d == "AUTO") {
                for (auto& hd : hardware_devices) {
                    auto& property = device_config[hd].as<ov::AnyMap>();
                    property.emplace(ov::device::properties(hd, ov::num_streams(nstream)));
                    if (!infer_precision.empty())
                        property.emplace(ov::hint::inference_precision(infer_precision));
                }
            } else if (d.find("GPU") != std::string::npos) {  // GPU
                device_config.emplace(ov::num_streams(nstream));
                if (!infer_precision.empty())
                    device_config.emplace(ov::hint::inference_precision(infer_precision));
            } else {  // CPU
                // insert inference precision to map device_config
                if (!infer_precision.empty())
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

struct ivsr {
    engine<ov_engine>* inferEngine;
    IVSRThread::IVSRThreadExecutor* threadExecutor;
    std::unordered_map<std::string, std::string> vsr_config;
    PatchConfig patchConfig;
    bool patchSolution;
    std::vector<size_t> input_data_shape;  // shape of input data

    ivsr()
        : threadExecutor(nullptr),
          patchSolution(false) {}

    // Define a constructor to initialize engine and other members if needed
    ivsr(engine<ov_engine>* engine,
         IVSRThread::IVSRThreadExecutor* executor,
         const std::unordered_map<std::string, std::string>& config,
         const PatchConfig& patch,
         std::vector<size_t> shape,
         bool sol = false)
        : inferEngine(engine),
          threadExecutor(executor),
          vsr_config(config),
          patchConfig(patch),
          patchSolution(sol),
          input_data_shape(std::move(shape)) {}
};

IVSRStatus ivsr_init(ivsr_config_t *configs, ivsr_handle *handle) {
    if (configs == nullptr || handle == nullptr) {
        ivsr_status_log(IVSRStatus::GENERAL_ERROR, "in ivsr_init");
        return IVSRStatus::GENERAL_ERROR;
    }

    // Configuration variables
    std::string model, device, batch, infer_precision;
    std::string verbose, custom_lib, cldnn_config;
    std::vector<size_t> reshape_settings, reso;
    size_t frame_width = 0, frame_height = 0;
    int reshape_h = 0, reshape_w = 0;
    std::unordered_map<std::string, std::string> config_map;
    size_t infer_request_num = 1;  // default infer_request_num set to 1
    const tensor_desc_t *input_tensor_desc = nullptr;
    const tensor_desc_t *output_tensor_desc = nullptr;

    // Parse input config
    while (configs != nullptr) {
        IVSRStatus unsupported_status = IVSRStatus::OK;
        std::string unsupported_output;

        switch (configs->key) {
            case IVSRConfigKey::INPUT_MODEL:
                model = std::string(static_cast<const char*>(configs->value));
                if (!checkFile(model)) {
                    unsupported_status = IVSRStatus::UNSUPPORTED_CONFIG;
                    unsupported_output = "INPUT_MODEL=" + std::string(static_cast<const char*>(configs->value));
                }
                std::cout << "[INFO] " << "Model Path:" << model << std::endl;
                break;
            case IVSRConfigKey::TARGET_DEVICE:
                device = static_cast<const char*>(configs->value);
                std::cout << "[INFO] " << "DEVICE:" << device << std::endl;
                break;
            case IVSRConfigKey::BATCH_NUM:
                batch = static_cast<const char*>(configs->value);
                break;
            case IVSRConfigKey::VERBOSE_LEVEL:
                verbose = static_cast<const char*>(configs->value);
                break;
            case IVSRConfigKey::CUSTOM_LIB:
                custom_lib = static_cast<const char*>(configs->value);
                if (!checkFile(custom_lib)) {
                    unsupported_status = IVSRStatus::UNSUPPORTED_CONFIG;
                    unsupported_output = "CUSTOM_LIB=" + std::string(static_cast<const char*>(configs->value));
                }
                break;
            case IVSRConfigKey::CLDNN_CONFIG:
                cldnn_config = static_cast<const char*>(configs->value);
                if (!checkFile(cldnn_config)) {
                    unsupported_status = IVSRStatus::UNSUPPORTED_CONFIG;
                    unsupported_output = "CLDNN_CONFIG=" + std::string(static_cast<const char*>(configs->value));
                }
                break;
            case IVSRConfigKey::PRECISION:
                infer_precision = static_cast<const char*>(configs->value);
                if (device.find("GPU") != std::string::npos) {
                    if (infer_precision != "f32" && infer_precision != "f16") {
                        ivsr_status_log(IVSRStatus::UNSUPPORTED_CONFIG, "for PRECISION=");
                        return IVSRStatus::UNSUPPORTED_CONFIG;
                    }
                } else {
                    if (infer_precision != "f32" && infer_precision != "bf16") {
                        ivsr_status_log(IVSRStatus::UNSUPPORTED_CONFIG, "for PRECISION=");
                        return IVSRStatus::UNSUPPORTED_CONFIG;
                    }
                }
                break;
            case IVSRConfigKey::RESHAPE_SETTINGS:
                reshape_settings = convert_string_to_vector(static_cast<const char*>(configs->value));
                //The layout of RESHAPE SETTINGS is NHW
                reshape_h = reshape_settings[ov::layout::height_idx(ov::Layout("NHW"))];
                reshape_w = reshape_settings[ov::layout::width_idx(ov::Layout("NHW"))];
                if (reshape_h % 2 != 0 || reshape_w % 2 != 0) {
                    ivsr_status_log(IVSRStatus::UNSUPPORTED_SHAPE, static_cast<const char*>(configs->value));
                    return IVSRStatus::UNSUPPORTED_SHAPE;
                }
                break;
            case IVSRConfigKey::INPUT_RES:
                reso = convert_string_to_vector(static_cast<const char*>(configs->value));
                frame_width  = reso[0];
                frame_height = reso[1];
                break;
            case IVSRConfigKey::INFER_REQ_NUMBER:
                try {
                    auto num = std::stoul(static_cast<const char*>(configs->value));
                    if (num > infer_request_num)
                        infer_request_num = num;
                    std::cout << "[INFO] Infer request num: " << infer_request_num << std::endl;
                } catch (const std::invalid_argument& e) {
                    std::cerr << "[ERROR] Invalid argument: " << static_cast<const char*>(configs->value) << std::endl;
                } catch (const std::out_of_range& e) {
                    std::cerr << "[ERROR] Out of range: " << static_cast<const char*>(configs->value) << std::endl;
                }
                break;
            case IVSRConfigKey::INPUT_TENSOR_DESC_SETTING:
                input_tensor_desc = static_cast<const tensor_desc_t *>(configs->value);
                break;
            case IVSRConfigKey::OUTPUT_TENSOR_DESC_SETTING:
                output_tensor_desc = static_cast<const tensor_desc_t *>(configs->value);
                break;
            default:
                unsupported_status = IVSRStatus::UNSUPPORTED_KEY;
                unsupported_output = std::to_string(configs->key);
                break;
        }

        ivsr_status_log(unsupported_status, unsupported_output.c_str());
        configs = configs->next;
    }

    if (!check_engine_config(model, device)) {
        return IVSRStatus::UNSUPPORTED_CONFIG;
    }

    if (frame_width == 0 || frame_height == 0) {
        ivsr_status_log(IVSRStatus::UNSUPPORTED_CONFIG, "please set INPUT_RES!");
        return IVSRStatus::UNSUPPORTED_CONFIG;
    }

    // Parse config for the inference engine
    std::map<std::string, ov::AnyMap> engine_configs;
    parse_engine_config(engine_configs, device, infer_precision, cldnn_config);

    // Initialize inference engine
    auto ovEng = new ov_engine(device,
                               model,
                               custom_lib,
                               engine_configs,
                               reshape_settings,
                               *input_tensor_desc,
                               *output_tensor_desc);

    IVSRStatus status = ovEng->init();
    if (status != IVSRStatus::OK) {
        ivsr_status_log(status, "in ivsr_init");
        return IVSRStatus::UNSUPPORTED_SHAPE;
    }

    auto res = ovEng->create_infer_requests(infer_request_num);
    if (res < 0) {
        std::cout << "[ERROR]: Failed to create infer requests!\n";
        return IVSRStatus::GENERAL_ERROR;
    }

    // Construct IVSRThreadExecutor object
    IVSRThread::Config executorConfig{"ivsr_thread_executor", 8};
    auto executor = new IVSRThread::IVSRThreadExecutor(executorConfig, ovEng);

    // Construct patch config
    tensor_desc_t input_tensor = {
        .precision = {0},
        .layout = {0},
        .tensor_color_format = {0},
        .model_color_format = {0},
        .scale = 0.0,
        .dimension = 0,
        .shape = {0}};
    ovEng->get_attr("model_inputs", input_tensor);

    tensor_desc_t output_tensor = {
        .precision = {0},
        .layout = {0},
        .tensor_color_format = {0},
        .model_color_format = {0},
        .scale = 0.0,
        .dimension = 0,
        .shape = {0}};
    ovEng->get_attr("model_outputs", output_tensor);

    PatchConfig patchConfig;
    int m_input_width = input_tensor.shape[ov::layout::width_idx(ov::Layout(input_tensor.layout))];;
    int m_input_height = input_tensor.shape[ov::layout::height_idx(ov::Layout(input_tensor.layout))];
    // hard code
    int nif = input_tensor.dimension == 5 ? input_tensor.shape[1] : 1;
    int m_output_width = output_tensor.shape[ov::layout::width_idx(ov::Layout(output_tensor.layout))];
    patchConfig.scale = m_output_width / m_input_width;
    patchConfig.patchHeight = m_input_height;
    patchConfig.patchWidth = m_input_width;
    patchConfig.dims = input_tensor.dimension;
    patchConfig.nif = nif;

#ifdef ENABLE_LOG
    std::cout << "[Trace]: " << patchConfig << std::endl;
#endif

    // Generate input data shape
    std::vector<size_t> input_res;
    input_res.push_back(frame_height);
    input_res.push_back(frame_width);

    // Use the parameterized constructor
    *handle = new ivsr(ovEng, executor, config_map, patchConfig, std::move(input_res));
    return IVSRStatus::OK;
}

IVSRStatus ivsr_process(ivsr_handle handle, char* input_data, char* output_data, ivsr_cb_t* cb) {
    if (input_data == nullptr) {
        ivsr_status_log(IVSRStatus::GENERAL_ERROR, "in ivsr_process - input_data is nullptr");
        return IVSRStatus::GENERAL_ERROR;
    }

    try {
        std::vector<int> int_shape;
        int_shape.reserve(handle->input_data_shape.size());  // Reserve space for efficiency
        std::transform(handle->input_data_shape.begin(),
                       handle->input_data_shape.end(),
                       std::back_inserter(int_shape),
                       [](size_t val) -> int {
                           return static_cast<int>(val);
                       });

        // Determine whether to apply the patch solution
        if (handle->patchConfig.patchHeight < int_shape[int_shape.size() - 2] ||
            handle->patchConfig.patchWidth < int_shape[int_shape.size() - 1]) {
            handle->patchSolution = true;
        }

        // Smart patch inference using a smart pointer for automatic memory management
        std::unique_ptr<SmartPatch> smartPatch(
            new SmartPatch(handle->patchConfig, input_data, output_data, int_shape, handle->patchSolution)
        );

        // Prepare data
        int res = smartPatch->generatePatch();
        if (res == -1) {
            ivsr_status_log(IVSRStatus::UNKNOWN_ERROR, "in SmartPatch::generatePatch");
            return IVSRStatus::UNKNOWN_ERROR;
        }

        auto patchList = smartPatch->getInputPatches();
        auto outputPatchList = smartPatch->getOutputPatches();

#ifdef ENABLE_PERF
        auto totalStartTime = Time::now();
#endif

        // Create infer requests based on patch list size
        size_t required_infer_requests = patchList.size();
        if (required_infer_requests > handle->inferEngine->get_infer_requests_size()) {
            auto res = handle->inferEngine->create_infer_requests(required_infer_requests);
            if (res < 0) {
                std::cout << "[ERROR]: Failed to create infer requests!\n";
                return IVSRStatus::GENERAL_ERROR;
            }
        }

        // Get data into infer task
        for (auto idx = 0u; idx < patchList.size(); ++idx) {
#ifdef ENABLE_LOG
            std::cout << "[Trace]: ivsr_process on patch: " << idx << std::endl;
#endif
            std::shared_ptr<InferTask> task = handle->threadExecutor->CreateTask(
                patchList[idx], outputPatchList[idx], InferFlag::AUTO);
            handle->threadExecutor->Enqueue(task);
        }

        // Wait for all tasks to finish
        handle->threadExecutor->wait_all(required_infer_requests);

#ifdef ENABLE_PERF
        double duration = get_duration_ms_till_now(totalStartTime);
        std::cout << "[PERF] Patch inference with memory copy - Latency: "
                  << double_to_string(duration) << "ms" << std::endl;
        std::cout << "[PERF] Patch inference with memory copy - Throughput: "
                  << double_to_string(handle->patchConfig.nif * 1000.0 / duration) << "FPS" << std::endl;
#endif

        // Restore output patches to images
        res = smartPatch->restoreImageFromPatches();
        if (res == -1) {
            ivsr_status_log(IVSRStatus::UNKNOWN_ERROR, "in SmartPatch::restoreImageFromPatches");
            return IVSRStatus::UNKNOWN_ERROR;
        }

        // Notify user
        cb->ivsr_cb(cb->args);

    } catch (const std::exception& e) {
        std::cout << "Error in ivsr_process: " << e.what() << std::endl;
        ivsr_status_log(IVSRStatus::EXCEPTION_ERROR, e.what());
        return IVSRStatus::UNKNOWN_ERROR;
    }

    return IVSRStatus::OK;
}

IVSRStatus ivsr_process_async(ivsr_handle handle, char* input_data, char* output_data, ivsr_cb_t* cb) {
    if (input_data == nullptr) {
        ivsr_status_log(IVSRStatus::GENERAL_ERROR, "in ivsr_process - input_data is nullptr");
        return IVSRStatus::GENERAL_ERROR;
    }

    try {
        std::vector<int> int_shape;
        int_shape.reserve(handle->input_data_shape.size());  // Reserve space for efficiency
        std::transform(handle->input_data_shape.begin(),
                       handle->input_data_shape.end(),
                       std::back_inserter(int_shape),
                       [](size_t val) -> int {
                           return static_cast<int>(val);
                       });

        // Determine whether to apply the patch solution
        if (handle->patchConfig.patchHeight < int_shape[int_shape.size() - 2] ||
            handle->patchConfig.patchWidth < int_shape[int_shape.size() - 1]) {
            handle->patchSolution = true;
        }

        // TODO: Now fallback to ivsr_process api when patch solution is needed
        if (handle->patchSolution) {
            return ivsr_process(handle, input_data, output_data, cb);
        }

        /* Uncomment: to use thread loop and internal task to process */
        // std::shared_ptr<InferTask> task =
        //   handle->threadExecutor->CreateTask(input_data, output_data, InferFlag::AUTO, cb);
        // handle->threadExecutor->Enqueue(task);

        handle->inferEngine->proc(input_data, output_data, cb);

    } catch (const std::exception& e) {
        std::cout << "Error in ivsr_process: " << e.what() << std::endl;
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
                    handle->vsr_config["model"] = static_cast<const char*>(configs->value);
                    break;
                case IVSRConfigKey::TARGET_DEVICE:
                    handle->vsr_config["device"] = static_cast<const char*>(configs->value);
                    break;
                case IVSRConfigKey::BATCH_NUM:
                    handle->vsr_config["batch_num"] = static_cast<const char*>(configs->value);
                    break;
                case IVSRConfigKey::VERBOSE_LEVEL:
                    handle->vsr_config["verbose_level"] = static_cast<const char*>(configs->value);
                    break;
                case IVSRConfigKey::CUSTOM_LIB:
                    handle->vsr_config["custom_lib"] = static_cast<const char*>(configs->value);
                    break;
                case IVSRConfigKey::CLDNN_CONFIG:
                    handle->vsr_config["cldnn_config"] = static_cast<const char*>(configs->value);
                    break;
                default:
                    break;
            }
            configs = configs->next;
        }

        // reconfig ov_engine ?

    } catch (const std::exception& e) {
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
            handle->inferEngine->get_attr("model_inputs", *(static_cast<tensor_desc_t *>(value)));
            break;
        }
        case IVSRAttrKey::OUTPUT_TENSOR_DESC:
        {
            handle->inferEngine->get_attr("model_outputs", *(static_cast<tensor_desc_t *>(value)));
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
            handle->inferEngine->get_attr("input_dims", dims);
            *((size_t *)value) = dims;
            break;
        }
        case IVSRAttrKey::OUTPUT_DIMS:
        {
            size_t dims = 0;
            handle->inferEngine->get_attr("output_dims", dims);
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
        auto p = handle->inferEngine->get_impl();
        if (p != nullptr)
            delete p;

        if (handle->threadExecutor) {
            delete handle->threadExecutor;
            handle->threadExecutor = nullptr;
        }
    } catch (const std::exception& e) {
        ivsr_status_log(IVSRStatus::EXCEPTION_ERROR, e.what());
        return IVSRStatus::UNKNOWN_ERROR;
    }

    delete handle;
    return IVSRStatus::OK;
}
