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
#include "ov_engine.hpp"

#include <unistd.h>

#include <cassert>
#include <cstring>
#include <irguard.hpp>

#include "omp.h"
#include "utils.hpp"

typedef std::chrono::high_resolution_clock Time;

const std::map<std::string, ov::element::Type> precision_string_to_ov = {
    {"fp32", ov::element::f32},
    {"f32", ov::element::f32},
    {"fp16", ov::element::f16},
    {"f16", ov::element::f16},
    {"i8", ov::element::i8},
    {"i16", ov::element::i16},
    {"i32", ov::element::i32},
    {"u8", ov::element::u8},
    {"u16", ov::element::u16},
};

const std::map<std::string, ov::preprocess::ColorFormat> color_format_string_to_ov = {
    {"BGR", ov::preprocess::ColorFormat::BGR},
    {"RGB", ov::preprocess::ColorFormat::RGB},
    {"I420_Single_Plane", ov::preprocess::ColorFormat::I420_SINGLE_PLANE},
    {"I420_Three_Planes", ov::preprocess::ColorFormat::I420_THREE_PLANES},
};

/*
 * IN: output
 * OUT: layout
 *
*/
static IVSRStatus get_default_layout(const ov::Output<ov::Node>& output, ov::Layout& layout) {
    size_t shape_size = output.get_partial_shape().size();
    switch (shape_size) {
    case 4:
        layout = ov::Layout("NCHW");
        break;
    case 5:
        layout = ov::Layout("NFCHW");
        break;
    default:
        std::cout << "not supported model input/output shape size\n";
        return GENERAL_ERROR;
    }
    return OK;
}

IVSRStatus ov_engine::init_impl() {
    if (custom_lib_ != "")
        instance_.add_extension(custom_lib_);
    // set property for ov instance
    for (auto&& item : configs_) {
        instance_.set_property(item.first, item.second);
    }
    // read model
    std::shared_ptr<ov::Model> model;
    try {
        model = instance_.read_model(model_path_);
    } catch (const std::exception& e) {
        model = irguard::load_model(instance_, model_path_);
    }

    bool multiple_inputs = false;
    if (model->inputs().size() == 5 && model->outputs().size() == 5)
        multiple_inputs = true;

    if (!reshape_settings_.empty()) {
        //get model input shape
        ov::PartialShape input_shape = model->inputs()[0].get_partial_shape();
#ifdef ENALBE_LOG
        std::cout << "input tensor shape is "<< input_shape.is_static()? "static: " : "dynamic: "
                  << input_shape << std::endl;
#endif

        size_t batch_index, channels_index, h_index, w_index;
        //get model input tensor layout
        ov::Layout input_layout = ov::layout::get_layout(model->inputs()[0]);
        if (input_layout.empty()) {
            get_default_layout(model->inputs()[0], input_layout);
            //ov::layout::set_layout(model->inputs()[0], input_layout);
        }
        batch_index = ov::layout::batch_idx(input_layout);
        channels_index = ov::layout::channels_idx(input_layout);
        h_index = ov::layout::height_idx(input_layout);
        w_index = ov::layout::width_idx(input_layout);

        // Assume the input reshape_settings_'s layout is NHW.
        // update input layer tensor batch/width/height with the value from reshape_settings_;
        input_shape[batch_index] = reshape_settings_[ov::layout::batch_idx(ov::Layout("NHW"))];
        input_shape[w_index] = reshape_settings_[ov::layout::width_idx(ov::Layout("NHW"))];
        input_shape[h_index] = reshape_settings_[ov::layout::height_idx(ov::Layout("NHW"))];
        //input_shape should be static now.
        assert(input_shape.is_static());

        //TODO: is this check for BasicVSR? Is it required anymore??
        if (input_shape.size() == 5) {
            if (input_shape[w_index].get_length() % 32 != 0) {
                std::cout << "[Error]: " << "Current model requires input widths to be divisible by 32" << std::endl;
                return UNSUPPORTED_SHAPE;
            }
        }

#ifdef ENABLE_LOG
        std::cout << "Reshape network to size = [" << input_shape[w_index].get_length()
			      << "x" << input_shape[h_index].get_length() << "] " << std::endl;
#endif
        // reshape the model with "static" shape.
        model->reshape({{model->inputs()[0].get_any_name(), input_shape.to_shape()}});

        if (multiple_inputs) {
            ov::Shape hidden_tensor_shape = input_shape.to_shape();
            hidden_tensor_shape[batch_index] = input_shape[batch_index].get_length();
            hidden_tensor_shape[channels_index] = 64;
            hidden_tensor_shape[h_index] = input_shape[h_index].get_length() / 4;
            hidden_tensor_shape[w_index] = input_shape[w_index].get_length() / 4;

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

    // PPP
    ov::preprocess::PrePostProcessor ppp = ov::preprocess::PrePostProcessor(model);
    ov::preprocess::InputInfo& input_info = ppp.input();
    ov::preprocess::OutputInfo& output_info = ppp.output();

    // layout is NCHW by default if can not get layout information
    ov::Layout layout;
    if (ov::layout::get_layout(model->inputs()[0]).empty()) {
        get_default_layout(model->inputs()[0], layout);
        input_info.model().set_layout(layout);
    }
    //after calling ppp::InputInfo.model().set_layout(); model->input_::layout is not set though.
    //std::cout << "model layout is " << ov::layout::get_layout(model->inputs()[0]).to_string() << endl;
    if (ov::layout::get_layout(model->outputs()[0]).empty()) {
        get_default_layout(model->outputs()[0], layout);
        output_info.model().set_layout(layout);
    }
    if (input_tensor_desc_.precision != nullptr) {
        input_info.tensor().set_element_type(precision_string_to_ov.at(std::string(input_tensor_desc_.precision)));
    }
    if (input_tensor_desc_.layout != nullptr) {
        const ov::Layout input_tensor_layout{input_tensor_desc_.layout};
        input_info.tensor().set_layout(input_tensor_layout);
    }
    if (output_tensor_desc_.precision != nullptr) {
        output_info.tensor().set_element_type(precision_string_to_ov.at(std::string(output_tensor_desc_.precision)));
    }
    if (output_tensor_desc_.layout != nullptr) {
        const ov::Layout output_tensor_layout{output_tensor_desc_.layout};
        output_info.tensor().set_layout(output_tensor_layout);
    }
    // convert color tensor_color_format->model_color_format
    if (strcmp(input_tensor_desc_.tensor_color_format, input_tensor_desc_.model_color_format) != 0) {
        input_info.tensor().set_color_format(
            color_format_string_to_ov.at(std::string(input_tensor_desc_.tensor_color_format)));
        input_info.preprocess().convert_color(
            color_format_string_to_ov.at(std::string(input_tensor_desc_.model_color_format)));
    }
    // ov 24.0 support convert color model_color_format->tensor_color_format
    // if (strcmp(input_tensor_desc_.tensor_color_format, input_tensor_desc_.model_color_format) != 0) {
    //    output_info.tensor().set_color_format(
    //        color_format_string_to_ov.at(std::string(input_tensor_desc_.tensor_color_format)));
    //    output_info.preprocess().convert_color(
    //        color_format_string_to_ov.at(std::string(input_tensor_desc_.model_color_format)));
    // }
    if ((input_tensor_desc_.scale - 1.0f) > 1e-6f) {
        // the input tensor precision should not be float
        assert(std::string(input_tensor_desc_.precision) == std::string("u8") ||
               std::string(input_tensor_desc_.precision) == std::string("u16"));
        input_info.preprocess().convert_element_type(ov::element::f32);
        input_info.preprocess().scale(input_tensor_desc_.scale);
        // PPP doesn't support un-scale, so
        // the precision of output tensor need to be float if the scale != 0 or 1
        output_info.tensor().set_element_type(ov::element::f32);
    }

    model = ppp.build();

    input_ = model->inputs()[0];
    output_ = model->outputs()[0];
    // compile model
    compiled_model_ = instance_.compile_model(model, device_);

#ifdef ENABLE_LOG
    std::cout << "[Trace]: " << "ov_engine init successfully" << std::endl;
#endif

    return OK;
}

IVSRStatus ov_engine::run_impl(InferTask::Ptr task) {
    // construct the input tensor
    if (task->inputPtr_ == nullptr || task->outputPtr_ == nullptr) {
        std::cout << "[Error]: "
                  << "invalid input buffer pointer" << std::endl;
        return GENERAL_ERROR;
    }
    auto inferReq = get_idle_request();

    inferReq->set_callback([wp = std::weak_ptr<inferReqWrap>(inferReq), task](std::exception_ptr ex) {
        auto request = wp.lock();
#ifdef ENABLE_PERF
        request->end_time();
        auto latency = request->get_execution_time_in_milliseconds();
        std::cout << "[PERF] Inference Latency: " << latency << "ms" << std::endl;
#endif

        if (ex) {
#ifdef ENABLE_LOG
            std::cout << "[Trace]: "
                      << "Exception in infer request callback " << std::endl;
#endif
            try {
                // std::rethrow_exception(ex);
                throw ex;
            } catch (const std::exception& e) {
                std::cout << "Caught exception \"" << e.what() << "\"\n";
            }
        }
        auto cbTask = task;

        request->call_back();
        // call application callback function
        cbTask->_callbackFunction(cbTask);
    });

#ifdef ENABLE_LOG
    std::cout << "[Trace]: "
              << "input: " << input_.get_element_type().get_type_name() << " " << input_.get_shape() << std::endl;
    std::cout << "[Trace]: "
              << "output: " << output_.get_element_type().get_type_name() << " " << output_.get_shape() << std::endl;
#endif

    ov::Tensor input_tensor(input_.get_element_type(), input_.get_shape(), task->inputPtr_);
    inferReq->set_input_tensor(input_tensor);

    ov::Tensor output_tensor(output_.get_element_type(), output_.get_shape(), task->outputPtr_);
    inferReq->set_output_tensor(output_tensor);
    inferReq->start_async();

#ifdef ENABLE_LOG
    std::cout << "[Trace]: "
              << "ov_engine run: start task inference" << std::endl;
#endif

    return OK;
}

IVSRStatus ov_engine::process_impl(void* input_data, void* output_data, void* cb) {
    // Check for valid input and output data pointers
    if (input_data == nullptr || output_data == nullptr) {
        std::cout << "[Error]: invalid input or output buffer pointer" << std::endl;
        return GENERAL_ERROR;
    }

    auto inferReq = get_idle_request();

    // Set callback for inference request
    inferReq->set_callback([wp = std::weak_ptr<inferReqWrap>(inferReq), cb](std::exception_ptr ex) {
        auto request = wp.lock();
#ifdef ENABLE_PERF
        request->end_time();
        auto latency = request->get_execution_time_in_milliseconds();
        std::cout << "[PERF] Inference Latency: " << latency << "ms" << std::endl;
#endif

        if (ex) {
#ifdef ENABLE_LOG
            std::cout << "[Trace]: Exception in infer request callback " << std::endl;
#endif
            try {
                // std::rethrow_exception(ex);
                throw ex;
            } catch (const std::exception& e) {
                std::cout << "Caught exception \"" << e.what() << "\"\n";
            }
        }

        request->call_back();

        // Check if the callback structure and function are valid, then call the function
        if (cb) {
            ivsr_cb_t* ivsr_cb = static_cast<ivsr_cb_t*>(cb);
            if (ivsr_cb->ivsr_cb) {
                ivsr_cb->ivsr_cb(ivsr_cb->args);
            }
        }
    });

#ifdef ENABLE_LOG
    std::cout << "[Trace]: input: " << input_.get_element_type().get_type_name() << " " << input_.get_shape() << std::endl;
    std::cout << "[Trace]: output: " << output_.get_element_type().get_type_name() << " " << output_.get_shape() << std::endl;
#endif

    // Construct input and output tensors
    ov::Tensor input_tensor(input_.get_element_type(), input_.get_shape(), input_data);
    inferReq->set_input_tensor(input_tensor);

    ov::Tensor output_tensor(output_.get_element_type(), output_.get_shape(), output_data);
    inferReq->set_output_tensor(output_tensor);

    // Start asynchronous inference
    inferReq->start_async();

#ifdef ENABLE_LOG
    std::cout << "[Trace]: ov_engine run: start task inference" << std::endl;
#endif

    return OK;
}

IVSRStatus ov_engine::create_infer_requests_impl(size_t requests_num) {
    if (requests_num < requests_.size()) {
        std::cout << "[ERROR]: "
                  << "please pass correct requests num.\n";
        return GENERAL_ERROR;
    }

    for (int id = requests_.size(); id < requests_num; ++id) {
        requests_.push_back(
            std::make_shared<inferReqWrap>(compiled_model_,
                                           id,
                                           std::bind(&ov_engine::put_idle_request, this, std::placeholders::_1)));
        idleIds_.push(id);
    }

    return OK;
}
