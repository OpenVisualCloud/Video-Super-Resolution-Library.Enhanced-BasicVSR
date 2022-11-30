// Copyright (C) 2022-2023 Intel Corporation

/**
 * @file ivsr.h
 * C API of Intel VSR SDK,
 * it is optimized on Intel Gen12+ GPU platforms.
 * Author: Renzhi.Jiang@intel.com
 */

#ifndef I_VSR_API_H
#define I_VSR_API_H

#include <stdio.h>
#include <stdint.h>

/**
 * @brief vsr context
 */
typedef struct ivsr *ivsr_handle;

typedef void (*ivsr_cb)(void* args);

/**
 * @brief Intel VSR SDK version.
 * 
 */
typedef struct ivsr_version {
    char *api_version; //!< A string representing ibasicvsr sdk version>
} ivsr_version_t;

/**
 * @brief Status for Intel VSR SDK
 * 
 */
typedef enum {
    OK              = 0,
    GENERAL_ERROR   = -1,
    UNSUPPORTED_KEY = -2
}IVSRStatus;

/**
 * @enum vsr sdk supported key.
 * 
 */
typedef enum {
    INPUT_MODEL   = 0x1,
    TARGET_DEVICE = 0x2,
    BATCH_NUM     = 0x3,
    VERBOSE_LEVEL = 0x4
}IVSRConfigKey;

typedef enum {
    IVSR_VERSION       = 0x1,
    INPUT_TENSOR_DESC  = 0x2,
    OUTPUT_TENSOR_DESC = 0x3
}IVSRAttrKey;

/**
 * @struct Intel VSR configuration.
 * 
 */
typedef struct ivsr_config {
    IVSRConfigKey key;
    const char *value;
    struct ivsr_config *next;
}ivsr_config_t;



#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief initialize the intel vsr sdk
 * 
 * @param configs configurations to initialize the intel vsr sdk.
 * @param handle handle used to process frames.
 * @return IVSRStatus 
 */
IVSRStatus ivsr_init(ivsr_config_t *configs, ivsr_handle *handle);

/**
 * @brief process function
 * 
 * @param handle vsr process handle.
 * @param input_data input data buffer
 * @param output_data output data buffer
 * @param cb  callback function.
 * @return IVSRStatus 
 */
IVSRStatus ivsr_process(ivsr_handle handle, char* input_data, char* output_data, ivsr_cb cb);

/**
 * @brief reset the configures for vsr
 * 
 * @param handle  vsr process handle
 * @param configs changed configurations for vsr.
 * @return IVSRStatus 
 */
IVSRStatus ivsr_reconfig(ivsr_handle handle, ivsr_config_t* configs);

/**
 * @brief get attributes 
 * 
 * @param handle vsr process handle
 * @param key indicate which type information to query.
 * @param value returned data.
 * @return IVSRStatus 
 */
IVSRStatus ivsr_get_attr(ivsr_handle handle, IVSRAttrKey key, void* value);

/**
 * @brief free created vsr handle and conresponding resources.
 * 
 * @param  handle vsr process handle.
 * @return IVSRStatus 
 */
IVSRStatus ivsr_deinit(ivsr_handle handle);

#ifdef __cplusplus
}
#endif

#endif //I_VSR_API_H