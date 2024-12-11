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
 * @file openvino_engine.h
 * openvino backend inference API,
 * it is the wrapper of backend inference API.
 */

#ifndef OV_ENGINE_HPP
#define OV_ENGINE_HPP

#include <condition_variable>
#include <queue>

#include "engine.hpp"
#include "openvino/core/layout.hpp"
#include "openvino/openvino.hpp"
#include "openvino/pass/make_stateful.hpp"
#include "openvino/pass/manager.hpp"

typedef std::function<void(size_t id)> CallbackFunction;
class inferReqWrap final {
public:
    using Ptr = std::shared_ptr<inferReqWrap>;
    explicit inferReqWrap(ov::CompiledModel& model, size_t id, CallbackFunction callback)
        : id_(id),
          request_(model.create_infer_request()),
          callback_(callback) {}

    void start_async() {
        startTime_ = Time::now();
        request_.start_async();
    }

    void end_time() {
        endTime_ = Time::now();
    }
    double get_execution_time_in_milliseconds() const {
        auto execTime = std::chrono::duration_cast<ns>(endTime_ - startTime_);
        return static_cast<double>(execTime.count()) * 0.000001;
    }

    void infer() {
        request_.infer();
        callback_(id_);
    }

    ov::Tensor get_tensor(const std::string& name) {
        return request_.get_tensor(name);
    }

    void set_input_tensor(const ov::Tensor& data) {
        request_.set_input_tensor(data);
    }

    void set_output_tensor(const ov::Tensor& data) {
        request_.set_output_tensor(data);
    }

    ov::Tensor get_output_tensor() {
        return request_.get_output_tensor();
    }

    ov::Tensor get_input_tensor() {
        return request_.get_input_tensor();
    }

    void set_tensor(const std::string& name, const ov::Tensor& data) {
        request_.set_tensor(name, data);
    }

    void set_callback(std::function<void(std::exception_ptr)> callback) {
        request_.set_callback(std::move(callback));
    }
    void call_back() {
        callback_(id_);
    }

private:
    ov::InferRequest request_;
    size_t id_;
    Time::time_point startTime_;
    Time::time_point endTime_;
    CallbackFunction callback_;
};

class ov_engine : public engine<ov_engine> {
public:
    ov_engine(std::string device,
              std::string model_path,
              std::string custom_lib,
              std::map<std::string, ov::AnyMap> configs,
              const std::vector<size_t>& reshape_settings,
              const tensor_desc_t input_tensor_desc,
              const tensor_desc_t output_tensor_desc)
        : engine(this),
          device_(device),
          model_path_(model_path),
          custom_lib_(custom_lib),
          configs_(configs),
          reshape_settings_(reshape_settings),
          input_tensor_desc_(input_tensor_desc),
          output_tensor_desc_(output_tensor_desc) {
        // init();
    }

    IVSRStatus init_impl();

    IVSRStatus run_impl(InferTask::Ptr task);

    IVSRStatus process_impl(void* input_data, void* output_data, void* cb = nullptr);

    template <typename T>
    IVSRStatus get_attr_impl(const std::string& key, T& value) {
        static_assert(std::is_same<T, ov::Shape>::value || std::is_same<T, size_t>::value ||
                          std::is_same<T, tensor_desc_t>::value,
                      "get_attr() is only supported for 'ov::Shape' and 'size_t' types");
        auto extend_shape = [](ov::Shape& shape, size_t dims) {
            if (shape.size() < dims)
                for (size_t i = shape.size(); i < dims; i++)
                    shape.insert(shape.begin(), 1);
        };

        if constexpr (std::is_same<T, tensor_desc_t>::value) {
            ov::Shape shape;
            std::string element_type;
            std::string layout;
            ov::Output<ov::Node> node;
            if (key == "model_inputs") {
                node = input_;
            } else if (key == "model_outputs") {
                node = output_;
            } else {
                return UNSUPPORTED_KEY;
            }

            layout = ov::layout::get_layout(node).to_string();
            shape = node.get_shape();
            element_type = node.get_element_type().get_type_name();
            memcpy((char*)value.precision, element_type.c_str(), element_type.size());
            memcpy((char*)value.layout, layout.c_str(), layout.size());
            value.dimension = shape.size();
            for (int i = 0; i < shape.size(); ++i) {
                value.shape[i] = shape[i];
            }
        } else if constexpr (std::is_same<T, size_t>::value) {
            if (key == "input_dims" || key == "output_dims") {
                const auto& shape = (key == "input_dims") ? input_.get_shape() : output_.get_shape();
                value = shape.size() < 5 ? 5 : shape.size();
            } else {
                return UNSUPPORTED_KEY;
            }
        }

        return OK;
    }

    inferReqWrap::Ptr get_idle_request() {
        std::unique_lock<std::mutex> lock(mutex_);
#ifdef ENABLE_LOG
        std::cout << "[Trace]: "
                  << "idleIds size: " << idleIds_.size() << std::endl;
#endif
        cv_.wait(lock, [this] {
            return idleIds_.size() > 0;
        });
        auto request = requests_.at(idleIds_.front());
        idleIds_.pop();
        return request;
    }

    void put_idle_request(size_t id) {
        std::unique_lock<std::mutex> lock(mutex_);
        idleIds_.push(id);
#ifdef ENABLE_LOG
        std::cout << "[Trace]: "
                  << "put_idle_request: idleIds size: " << idleIds_.size() << std::endl;
#endif
        cv_.notify_one();
    }

    void wait_all_impl() {
#ifdef ENABLE_LOG
        std::cout << "[Trace]: "
                  << "ov_engine wait_all: "
                  << "idleIds_ size:" << idleIds_.size() << " requests_ size:" << requests_.size() << std::endl;
#endif
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] {
            return idleIds_.size() == requests_.size();
        });
    }

    IVSRStatus create_infer_requests_impl(size_t requests_num);

    const size_t get_infer_requests_size_impl() {
        return requests_.size();
    }

    ~ov_engine() {
        requests_.clear();
    }

private:
    std::string device_;
    std::queue<size_t> idleIds_;
    std::vector<inferReqWrap::Ptr> requests_;
    std::mutex mutex_;
    std::condition_variable cv_;
    // configurations for openvino instances.
    std::map<std::string, ov::AnyMap> configs_;
    ov::Core instance_;
    ov::CompiledModel compiled_model_;
    std::vector<size_t> reshape_settings_;
    tensor_desc_t input_tensor_desc_;
    tensor_desc_t output_tensor_desc_;

    std::string custom_lib_;
    std::string model_path_;

    ov::Output<ov::Node> input_;
    ov::Output<ov::Node> output_;
};

#endif  // OV_ENGINE_HPP
