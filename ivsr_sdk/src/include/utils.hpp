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
 * @brief basic definition for ivsr
 *
 */

#ifndef UTILS_HPP
#define UTILS_HPP

#include <sys/stat.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#ifndef _WIN32
#    include <dirent.h>
#    include <unistd.h>
#endif

#include "ivsr.h"

/**
 * @enum IBASICVSRStatus
 * @brief This eunum contains codes for all possible returen value of iBascVSR.
 */
typedef enum { SUCCESS = 0, ERROR = -1 } IBasicVSRStatus;

typedef enum { NCHW = 0, NHWC = 1, BNCHW = 2 } InputLayout;

typedef std::chrono::high_resolution_clock Time;
typedef std::chrono::nanoseconds ns;

inline double get_duration_ms_till_now(Time::time_point& startTime) {
    return std::chrono::duration_cast<ns>(Time::now() - startTime).count() * 0.000001;
};

inline std::string double_to_string(const double number) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << number;
    return ss.str();
};

inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;

    while (getline(ss, item, delim)) {
        result.push_back(item);
    }
    return result;
};

inline void ivsr_status_log(IVSRStatus status, const char* log) {
    switch (status) {
    case IVSRStatus::GENERAL_ERROR:
        std::cout << "[General Error] NULL pointer exception " << log << "." << std::endl;
        break;
    case IVSRStatus::UNSUPPORTED_KEY:
        std::cout << "[Error] Unsupported keys " << log << ", please check the input keys." << std::endl;
        break;
    case IVSRStatus::UNSUPPORTED_CONFIG:
        std::cout << "[Error] Unsupported configs " << log << ", please check the input configs." << std::endl;
        break;
    case IVSRStatus::UNKNOWN_ERROR:
        std::cout << "[Unknown Error] Process failed " << log << "." << std::endl;
        break;
    case IVSRStatus::EXCEPTION_ERROR:
        std::cout << "[Exceptoin] Exception occurred " << log << "." << std::endl;
        break;
    case IVSRStatus::UNSUPPORTED_SHAPE:
        std::cout << "[Error] Unsupported input shape " << log << ", please check the input frame's size." << std::endl;
        break;

    default:
        break;
    }
}

inline bool checkFile(const std::string& path) {
    struct stat path_stat;
    if (stat(path.c_str(), &path_stat) == 0) {
        // Check if the path is a regular file (not a directory or link, etc.)
        if (S_ISREG(path_stat.st_mode)) {
            return true;  // The path is a regular file.
        }
        std::cout << "Input " << path << " is not a regular file!" << std::endl;
        return false;  // The path is not a regular file.
    }

    std::cout << "Unable to get file status for " << path << std::endl;
    return false;  // stat call failed, perhaps the file doesn't exist or we don't have permission to access it.
}

#endif
