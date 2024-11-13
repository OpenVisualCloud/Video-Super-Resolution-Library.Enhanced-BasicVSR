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

#ifndef INFER_TASK_HPP
#define INFER_TASK_HPP

#include <memory>
#include <functional>
#include <string>
#include "ivsr.h"
#include "utils.hpp"

typedef enum {
    GPU = 0x0, //GPU
    CPU = 0x1, //CPU
    AUTO = 0x2, //AUTO, will allow backend engine determined automatically
} InferFlag;

typedef std::function<void()> QueueCallbackFunction;

class InferTask {
public:
    using Ptr = std::shared_ptr<InferTask>;
    //construct function
    InferTask(char* inBuf, char* outBuf,  QueueCallbackFunction callbackQueue, InferFlag flag):
        inputPtr_(inBuf), outputPtr_(outBuf), _callbackQueue(callbackQueue), flag_(flag) {}

    InferFlag getInferFlag() { return flag_; }

    double get_execution_time_in_milliseconds() const {
        auto execTime = std::chrono::duration_cast<ns>(_endTime - _startTime);
        return static_cast<double>(execTime.count()) * 0.000001;
    }
    
public:
    QueueCallbackFunction _callbackQueue;
    InferFlag flag_ = InferFlag::GPU; //Default will use GPU to do inference task
    char* inputPtr_ = nullptr; //input buffer ptr
    char* outputPtr_ = nullptr; //output buffer pointer
    Time::time_point _startTime;
    Time::time_point _endTime;
};

#endif //INFER_TASK_HPP
