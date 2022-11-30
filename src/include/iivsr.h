// Copyright (C) 2022-2023 Intel Corporation

/**
 * @file iivsr.h
 * C API of Internal Intel VSR SDK,
 * it is optimized on Intel Gen12+ GPU platforms.
 */

#ifndef I_I_VSR_API_H
#define I_I_VSR_API_H

#include <stdint.h>
#include <stdio.h>

/**
 * @struct basicvsr context
 * 
 */
typedef struct ibasicvsr ibasicvsr_t;
typedef struct ibasicvsr_infer ibasicvsr_infer_t;

/**
 * @struct ibasicvsr_version
 * represents the api version that reflects the optmization stage.
 * 
 */
typedef struct ibasicvsr_version {
    char *api_version; //!< A string representing ibasicvsr sdk version>
} ibasicvsr_version_t;


/**
 * @struct ibasicvsr_config
 * represents configuration information that for basicvsr solution.
 * 
 */
typedef struct ibasicvsr_config {
    const char *key;  //!< A configuration key
    const char *value;  //!< A configuration value
}ibasicvsr_config_t;

/**
 * @enum IBASICVSRStatus
 * @brief This eunum contains codes for all possible returen value of iBascVSR.
 */
typedef enum {
    OK = 0,
    GENERAL_ERROR = -1,
}IBasicVSRStatus;

/**
 * @brief struct ibasicvsr completion call back function
 * completetion callback definition
 */
typedef struct ibasicvsr_callback {
    void (*completeCallBackFunc)(void *args);
    void *args;
} ibasicvsr_callback_t;

/**
 * @enum iBasicVSR supported tensor layout
 * 
 */
typedef enum {
    ANY = 0,   //!< "ANY" layout
    NCHW = 1,  //!< "NCHW" layout
    NHWC = 2,  //!< "NHWC" layout
} ibasicvsr_layout_e;

/**
 * @enum iBasicVSR supported precision
 * 
 */
typedef enum {
    UNSUPPORTED = 255, //!< unsupported precision.
    MIXED = 0, //!< Mixed precision.
    FP32 = 1,  //!< float precision.
    FP16 = 2,  //!< half float precision.
    FP64 = 3, //!< 64bit float precision.
    I16  = 4, //!< 16bit int precision.
    U16  = 5, //!< 16bit unsigned int precision
    I32  = 6, //!< 32bit int precision.
    U32  = 7, //!< 32bit unsigned int precision.
    I8   = 8, //!< 8bit int precision.
    U8   = 9, //!< 8bit unsigned precision.
    I4   = 10, //!< 4bit int precision.
    U4   = 11, //!< 4bit unsigned precision.
} ibasicvsr_precision_e;

/**
 * @struct iBasicVSR tensor dims
 * 
 */
typedef struct ibasicvsr_dims {
    size_t ranks; //!< a rank representing a number of dimensions.
    size_t dims[8]; //!< An array of dimensions.
} ibasicvsr_dims_t;

/**
 * @brief iBasicVSR tensor description.
 * 
 */
typedef struct ibasicvsr_tensor_desc {
    ibasicvsr_layout_e layout;
    ibasicvsr_precision_e precision;
    ibasicvsr_dims_t dims;
}ibasicvsr_tensor_desct_t;


typedef enum {
    iBASICVSR_DEBUG = 0,
    iBASICVSR_INFO  = 1,
    iBASICVSR_WARN  = 2,
    iBASICVSR_ERROR = 3,
}ibasicvsr_verbose_e;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief get the ibasicvsr version information/
 * 
 * @return ibasicvsr_version_t 
 */
ibasicvsr_version_t ibasicvsr_version(void);

/**
 * @brief create context for basicvsr
 * 
 * @param model_file model name
 * @param basicvsr basicvsr context
 * @return IBasicVSRStatus basicvsr context create status.
 */
IBasicVSRStatus ibasicvsr_create(const char *model_file, ibasicvsr_t **basicvsr);

/**
 * @brief release the created basicvsr context.
 * 
 * @param basicvsr 
 */
IBasicVSRStatus ibasicvsr_free(ibasicvsr_t **basicvsr);

/**
 * @brief basicvsr context initialization
 * 
 * @param basicvsr basicvsr context
 * @param configs  basicvsr configurations
 * @param device_name device name
 * @return IBasicVSRStatus 
 */
IBasicVSRStatus ibasicvsr_init(ibasicvsr_t *basicvsr, const char* device_name, ibasicvsr_config_t* configs);

/**
 * @brief get BasicVSR supported configuration keys
 * 
 * @param keys supported keys.
 * @return IBasicVSRStatus 
 */
IBasicVSRStatus ibasicvsr_supported_config_keys(const char **keys);

/**
 * @brief get iBasicVSR input tensor description
 * 
 * @param input_desc input tensor descrition.
 * @return IBasicVSRStatus 
 */
IBasicVSRStatus ibasicvsr_input_desc(ibasicvsr_tensor_desct_t *input_desc);

/**
 * @brief get iBasicVSR output tensor description
 * 
 * @param output_desc output tenrspr description.
 * @return IBasicVSRStatus 
 */
IBasicVSRStatus ibasicvsr_output_desc(ibasicvsr_tensor_desct_t *output_desc);

/**
 * @brief create inference task
 * 
 * @param basicvsr iBasicVSR context
 * @param infer_task infer task
 * @return IBasicVSRStatus 
 */
IBasicVSRStatus ibasicvsr_create_infer_task(ibasicvsr *basicvsr, ibasicvsr_infer_t ** infer_task);

/**
 * @brief free the created iBasicVSR inference task.
 * 
 * @param basicvsr basicvsr context
 * @param infer_task created inference request.
 * @return IBasicVSRStatus 
 */
IBasicVSRStatus ibasicvsr_free_infer_task(ibasicvsr *basicvsr, ibasicvsr_infer_t *infer_task);

/**
 * @brief set input for created inference task.
 * 
 * @param infer_task inference task.
 * @param handle buffer pointer to the input data.
 * @param gpu_memory flag to indicate it's gpu memory or not.
 * @return IBasicVSRStatus 
 */
IBasicVSRStatus ibasicvsr_set_infer_input(ibasicvsr_infer_t* infer_task, const void *handle, bool gpu_memory);

/**
 * @brief get output data for finished inference task.
 * 
 * @param infer_task created inference task
 * @param data_ptr  data pointer to the output buffer.
 * @return IBasicVSRStatus 
 */
IBasicVSRStatus ibasicvsr_get_infer_output(ibasicvsr_infer_t *infer_task, void* data_ptr);

/**
 * @brief set callback function for ibasicvsr
 * 
 * @param infer_task  created inference task
 * @param cb call back function for created inference task.
 * @return IBasicVSRStatus 
 */
IBasicVSRStatus ibasicvsr_infer_callback(ibasicvsr_infer_t *infer_task, ibasicvsr_callback_t *cb);

/**
 * @brief asychronously infer the task
 * 
 * @param basicvsr created basicvsr context.
 * @param infer_task  created infer task.
 * @return IBasicVSRStatus 
 */
IBasicVSRStatus ibasicvsr_start_infer_task(ibasicvsr_t* basicvsr, ibasicvsr_infer_t* infer_task);

/**
 * @brief set iBasicVSR log level.
 * 
 * @param verbose 
 * @return IBasicVSRStatus 
 */
IBasicVSRStatus ibasicvsr_verbose(ibasicvsr_verbose_e verbose);

/**
 * @brief set batch number for iBasicVSR.
 * 
 * @param batch 
 * @return IBasicVSRStatus 
 */
IBasicVSRStatus ibasicvsr_set_batch(uint32_t batch);
#ifdef __cplusplus
}
#endif


#endif //I_I_VSR_API_H