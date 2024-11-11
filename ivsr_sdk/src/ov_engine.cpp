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

/**
 * @file openvino_engine.cpp
 * openvino backend inference implementation
 * it is the wrapper of backend inference API.
 */
#include <unistd.h>
#include <cstring>
#include <cassert>
#include "ov_engine.hpp"
#include "utils.hpp"
#include "omp.h"

#include <irguard.hpp>

typedef std::chrono::high_resolution_clock Time;

IBasicVSRStatus ov_engine::init_impl()
{
    if (custom_lib_ != "")
        instance_.add_extension(custom_lib_);
    //set property for ov instance
    for (auto&& item : configs_) {
        instance_.set_property(item.first, item.second);
    }
    //read model
    std::shared_ptr<ov::Model> model;
    try {
        model = instance_.read_model(model_path_);
    } catch (const std::exception& e) {
        model = irguard::load_model(instance_, model_path_);
    }

    input_ = model->inputs()[0];
    output_ = model->outputs()[0];

    bool multiple_inputs = false;
    if (model->inputs().size() == 5 && model->outputs().size() == 5)
        multiple_inputs = true;

    if (!reshape_settings_.empty()) {
        // get_shape() only can be called by static shape, openvino will check the shape size during reshape operation
        //ov::Shape tensor_shape = input_.get_shape();
        //assert(tensor_shape.size() == reshape_settings_.size());

        ov::Shape tensor_shape = reshape_settings_;

        size_t batch_index, channels_index, h_index, w_index;
        if (multiple_inputs) {
            const ov::Layout model_layout{"NCHW"};
            batch_index = ov::layout::batch_idx(model_layout);
            channels_index = ov::layout::channels_idx(model_layout);
            h_index = ov::layout::height_idx(model_layout);
            w_index = ov::layout::width_idx(model_layout);
        }

#ifdef ENABLE_LOG
        std::cout << "Reshape network to the image size = [" << reshape_settings_[reshape_settings_.size() - 2] << "x"
                  << reshape_settings_[reshape_settings_.size() - 1] << "] " << std::endl;
#endif
        model->reshape({{model->inputs()[0].get_any_name(), tensor_shape}});

        if (multiple_inputs) {
            ov::Shape hidden_tensor_shape = reshape_settings_;
            hidden_tensor_shape[batch_index] = tensor_shape[batch_index];
            hidden_tensor_shape[channels_index] = 64;
            hidden_tensor_shape[h_index] = tensor_shape[h_index] / 4;
            hidden_tensor_shape[w_index] = tensor_shape[w_index] / 4;

            for (int i = 1; i < model->inputs().size(); ++i)
                model->reshape({{model->inputs()[i].get_any_name(), hidden_tensor_shape}});
        }
    }

    // build stateful model
    if (multiple_inputs) {
        // Time::time_point start_time = Time::now();
        std::map<std::string, std::string> tensor_names;
        const auto& inputs = model->inputs();
        const auto& outputs = model->outputs();
        for (int i = 1; i < inputs.size(); ++i) {
            std::string hidden_inp_name = inputs[i].get_any_name();
            std::string hidden_out_name = outputs[i].get_any_name();
            tensor_names[hidden_inp_name] = hidden_out_name;
        }

        ov::pass::Manager manager;
        manager.register_pass<ov::pass::MakeStateful>(tensor_names);
        manager.run_passes(model);
        // Time::time_point end_time = Time::now();
        // auto execTime = std::chrono::duration_cast<ns>(end_time - start_time);
        // std::cout << "The exec time of making stateful model is " << execTime.count() * 0.000001 << "ms\n";
    }

    //compile model
    compiled_model_ = instance_.compile_model(model, device_);

#ifdef ENABLE_LOG
    std::cout << "[Trace]: " << "ov_engine init successfully" << std::endl;
#endif

    return SUCCESS;
}

IBasicVSRStatus ov_engine::run_impl(InferTask::Ptr task) {
    //construct the input tensor
    if(task->inputPtr_ == nullptr || task->outputPtr_ == nullptr) {
        std::cout << "[Error]: " << "invalid input buffer pointer" << std::endl;
        return ERROR;
    }
    auto inferReq = get_idle_request();

    inferReq->set_callback([wp = std::weak_ptr<inferReqWrap>(inferReq),task](std::exception_ptr ex) {
        auto request = wp.lock();
#ifdef ENABLE_PERF
	request->end_time();
        auto latency = request->get_execution_time_in_milliseconds();
        size_t frame_num = (request->get_input_tensor()).get_shape()[(request->get_input_tensor().get_shape()).size()-4];
        std::cout << "[PERF] " << "Inference Latency: " << latency << "ms, Throughput: " << double_to_string(frame_num * 1000.0 / latency) << "FPS" << std::endl;
#endif

        if(ex) {
#ifdef ENABLE_LOG
        std::cout << "[Trace]: " << "Exception in infer request callback " << std::endl;
#endif
            try{
                // std::rethrow_exception(ex);
                throw ex;
            } catch(const std::exception& e) {
                std::cout << "Caught exception \"" << e.what() << "\"\n";
            }
        }
        auto cbTask = task;

        request->call_back();
        //call application callback function
	cbTask->_callbackQueue();
    });

#ifdef ENABLE_LOG
    std::cout << "[Trace]: " << "input: " << input_.get_element_type().get_type_name() << " " << input_.get_shape() << std::endl;
    std::cout << "[Trace]: " << "output: " << output_.get_element_type().get_type_name() << " " << output_.get_shape() << std::endl;
#endif

    ov::Tensor input_tensor(input_.get_element_type(), input_.get_shape(), task->inputPtr_);
    inferReq->set_input_tensor(input_tensor);

    ov::Tensor output_tensor(output_.get_element_type(), output_.get_shape(), task->outputPtr_);
    inferReq->set_output_tensor(output_tensor);
    inferReq->start_async();

#ifdef ENABLE_LOG
    std::cout << "[Trace]: " << "ov_engine run: start task inference" << std::endl;
#endif

    return SUCCESS;
}

IBasicVSRStatus ov_engine::create_infer_requests_impl(size_t requests_num) {
    if (requests_num < requests_.size()) {
        std::cout << "[ERROR]: " << "please pass correct requests num.\n";
        return ERROR;
    }

    for (int id = requests_.size(); id < requests_num; ++id) {
        requests_.push_back(std::make_shared<inferReqWrap>(compiled_model_ ,id,std::bind(&ov_engine::put_idle_request,
                                                                        this,
                                                                        std::placeholders::_1)));
        idleIds_.push(id);
    }

    return SUCCESS;
}