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
 * @file engine.h
 * backend inference API,
 * it is the wrapper of backend inference API.
 */

#ifndef COMMON_ENGINE_HPP
#define COMMON_ENGINE_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "InferTask.hpp"
#include "utils.hpp"

using namespace std;

template <typename Derived>
class engine {
private:
    using InitFunc = std::function<IVSRStatus()>;
    using RunFunc = std::function<IVSRStatus(InferTask::Ptr)>;
    using ProcFunc = std::function<IVSRStatus(void*, void*, void*)>;
    using WaitAllFunc = std::function<void()>;
    using CreateInferRequestsFunc = std::function<IVSRStatus(size_t)>;
    using GetInferRequestsSizeFunc = std::function<size_t()>;

    InitFunc init_func;
    RunFunc run_func;
    ProcFunc proc_func;
    WaitAllFunc wait_all_func;
    CreateInferRequestsFunc create_infer_requests_func;
    GetInferRequestsSizeFunc get_infer_requests_size_func;

    Derived* _derived = nullptr;

public:
    engine(Derived* derived)
        : init_func([=]() -> IVSRStatus {
              return _derived->init_impl();
          }),
          run_func([=](InferTask::Ptr task) -> IVSRStatus {
              return _derived->run_impl(task);
          }),
          proc_func([=](void* input, void* output, void* cb) -> IVSRStatus {
              return _derived->process_impl(input, output, cb);
          }),
          wait_all_func([=]() {
              _derived->wait_all_impl();
          }),
          create_infer_requests_func([=](size_t requests_num) -> IVSRStatus {
              return _derived->create_infer_requests_impl(requests_num);
          }),
          get_infer_requests_size_func([=]() -> size_t {
              return _derived->get_infer_requests_size_impl();
          }),
          _derived(derived) {}
    
    // Default constructor
    engine() = default;

    IVSRStatus init() {
        return init_func();
    }

    IVSRStatus run(InferTask::Ptr task) {
        return run_func(task);
    }

    IVSRStatus proc(void* input_data, void* output_data, void* cb) {
        return proc_func(input_data, output_data, cb);
    }

    template <typename T>
    IVSRStatus get_attr(const std::string& key, T& value) {
        return _derived->get_attr_impl(key, value);
    }

    void wait_all() {
        return wait_all_func();
    }

    IVSRStatus create_infer_requests(size_t requests_num) {
        return create_infer_requests_func(requests_num);
    }

    size_t get_infer_requests_size() {
        return get_infer_requests_size_func();
    }

    Derived* get_impl() const {
        return _derived;
    }
};

#endif  // COMMON_ENGINE_HPP
