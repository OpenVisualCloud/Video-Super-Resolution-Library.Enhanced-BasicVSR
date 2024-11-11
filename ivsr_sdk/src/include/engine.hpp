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

#include "utils.hpp"
#include "InferTask.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

using namespace std;

template<typename Derived>
class engine {
private:
    // Function objects for type-erased calls to interface methods
    std::function<IBasicVSRStatus()> init_func;
    std::function<IBasicVSRStatus(InferTask::Ptr task)> run_func;
    std::function<void()> wait_all_func;
    Derived* _derived = nullptr;

public:
    // Template constructor binds the provided methods of the derived engine implementation
    engine(Derived* derived)
        : _derived(derived),
          init_func([=]() -> IBasicVSRStatus { return _derived->init_impl(); }),
          run_func([=](InferTask::Ptr task) -> IBasicVSRStatus { return _derived->run_impl(task); }),
          wait_all_func([=]() { _derived->wait_all_impl(); })
    {}

    engine() = default;

    // Public interface methods call the type-erased std::function members
    IBasicVSRStatus init() {
        return init_func();
    }

    IBasicVSRStatus run(InferTask::Ptr task) {
        return run_func(task);
    }

    // The templated get_attr method delegates to the derived class's method
    template <typename T>
    IBasicVSRStatus get_attr(const std::string& key, T& value) {
        // Using CRTP style static_cast to delegate to the actual implementation provided by the derived class
        // For this to work, the derived class must implement get_attr_impl with the appropriate signature
        return _derived->get_attr_impl(key, value);
    }

    void wait_all() {
        wait_all_func();
    }

    Derived* get_impl() const { return _derived; }

    IBasicVSRStatus create_infer_requests(size_t requests_num) {
        return _derived->create_infer_requests_impl(requests_num);
    }

    size_t get_infer_requests_size() {
        return _derived->get_infer_requests_size_impl();
    }
};

#endif //COMMON_ENGINE_HPP
