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
#include"ivsr_smart_patch.hpp"
#include<cmath>
#include<stdlib.h>
#include<unistd.h>
#include "omp.h"
#include<string.h>


std::vector<std::vector<int>> calculatePatchCoordinateList(int oriH, int oriW, int cropSize[], int blockSize[]){
    int cropHeight = cropSize[0];
    int cropWidth = cropSize[1];
    int interHeight = (cropHeight * blockSize[0] - oriH) / (blockSize[0] - 1);
    int lastFillHeight = (cropHeight * blockSize[0] - oriH) % (blockSize[0] - 1);
    int interWidth = (cropWidth * blockSize[1] - oriW) / (blockSize[1] - 1);
    int lastFillWidth = (cropWidth * blockSize[1] - oriW) % (blockSize[1] - 1);
    std::vector<std::vector<int> > cropCoordinateList;
    for(int i = 0; i < blockSize[0]; i ++){
        for(int j = 0; j < blockSize[1]; j ++){
            int x1 = (cropHeight - interHeight) * i;
            int y1 = (cropWidth - interWidth) * j;
            if(i == blockSize[0] - 1) x1 -= lastFillHeight;
            if(j == blockSize[1] - 1) y1 -= lastFillWidth;
            cropCoordinateList.push_back({x1, y1, x1 + cropHeight, y1 + cropWidth});
        }
    }
    return cropCoordinateList;
}


float* fill_patch(std::vector<int> patchCorners, float* inputBuf, std::vector<int> inputDims, float* patchBuf){
    int x0 = patchCorners[0], y0 = patchCorners[1];
    int x1 = patchCorners[2], y1 = patchCorners[3];
    float * patch_ptr = patchBuf;
    if(inputDims.size() < 5){
        std::cout<<"Error: expected inputs in 5 dims(B,N,C,H,W), but got "<< inputDims.size() <<"dims." <<std::endl;
        return patch_ptr;
    }
    
    // 1x3x3x1080x1920
    int B = inputDims[0], N = inputDims[1], C = inputDims[2], H = inputDims[3], W = inputDims[4];
    int inp_sW = 1;
    int inp_sH = W;
    int inp_sC = H * W;
    int inp_sN = C * H * W;
    int inp_sB = N * C * H * W;

    
    if(x0 < 0 || x1 > H || y0 < 0 || y1 > W){
        std::cout<<"Error: illegal patch corners:(" << x0 << ", " << y0 << ", " << x1 << ", " << y1 << ")" <<std::endl;
        return patch_ptr;
    }
    
    // loop over each pixel
    for(int b = 0; b < B; ++b){
        float * inp_ptr_B = inputBuf + b * inp_sB;
        for(int n = 0; n < N; ++n) {
            float *inp_ptr_N = inp_ptr_B + n * inp_sN;
            for(int c = 0; c < C; ++c){
                float *inp_ptr_C = inp_ptr_N + c * inp_sC;
                for (int h = x0; h < x1; ++h) {
                    float *inp_ptr_H = inp_ptr_C + h * inp_sH;
                    for(int w = y0; w < y1; ++w) {
                        *patch_ptr = *(inp_ptr_H + w);
                        patch_ptr++ ;
                    }
                }
            }
        }
    }
    
    return patch_ptr;

}

void fill_image(std::vector<std::vector<int>> patchCorners, char* imgBuf, \
    std::vector<int> patchDims, std::vector<int> imgDims, std::vector<char*> patchList){
    int pB = patchDims[0], pN = patchDims[1], pC = patchDims[2], pH = patchDims[3], pW = patchDims[4];
    int iB = imgDims[0], iN = imgDims[1], iC = imgDims[2], iH = imgDims[3], iW = imgDims[4]; // imgDims?
    int patch_sW = 1;
    int patch_sH = pW;
    int patch_sC = pH * pW;
    int patch_sN = pC * pH * pW;
    int patch_sB = pN * pC * pH * pW;

    int img_sW = 1;
    int img_sH = iW;
    int img_sC = iH * iW;
    int img_sN = iC * iH * iW;
    int img_sB = iN * iC * iH * iW;
    
    size_t outputpixels = iB * iN * iC * iH * iW;
    int * pixelCounter = new int[outputpixels]();

    float * img_ptr =(float *)imgBuf;

    // for each patch
    for (int idx = 0; idx < patchCorners.size(); ++idx){
        float* patchPtr = (float*)patchList[idx];
        auto patchCorner = patchCorners[idx];   
        
        // -loop over each pixel on patch and  find out the corresponding pixel in image
        for(int b = 0; b < pB; ++b){
            float* patch_ptr_B = patchPtr + b * patch_sB;
            float* img_ptr_B = img_ptr + b * img_sB;
            for(int n = 0; n < pN; ++n) {
                float* patch_ptr_N = patch_ptr_B + n * patch_sN;
                float* img_ptr_N = img_ptr_B + n * img_sN;
                for(int c = 0; c < pC; ++c){
                    float *patch_ptr_C = patch_ptr_N + c * patch_sC;
                    float *img_ptr_C = img_ptr_N + c * img_sC;

                    float *img_ptr_sH = img_ptr_C + patchCorner[0] * img_sH; // start H pointer in image for current patch 
                    for (int h = 0; h < pH; ++h) {
                        float *patch_ptr_H = patch_ptr_C + h * patch_sH;
                        float *img_ptr_H = img_ptr_sH + h * img_sH;

                        float * img_ptr_W = img_ptr_H + patchCorner[1]; // start W pointer in image for current patch 
                        for(int w = 0; w < pW; ++w) {
                            *img_ptr_W += *(patch_ptr_H + w);
                            *(pixelCounter + (img_ptr_W - img_ptr)) += 1;
                            img_ptr_W++;
                        }
                    }
                }
            }
        }
    }

    // average each pixel
    for(int id = 0; id < outputpixels; id++){
        *(img_ptr + id) /= *(pixelCounter + id);
    }

    delete[] pixelCounter;
}

SmartPatch::SmartPatch(PatchConfig config, char* inBuf, char* outBuf, std::vector<int> inputShape, bool flag)
    :_inputPtr(inBuf),
    _outputPtr(outBuf),
    _config(config),
    _inputShape(inputShape),
    flag(flag)
    {
        if (flag){
            size_t inputSize = 1;
            for( auto shape : _inputShape){
                inputSize *= shape;
            }
            inputSize  = sizeof(float) * inputSize;
            _patchInputPtr = new float[inputSize];
            inputSize  = inputSize * _config.scale * _config.scale;
            _patchOutputPtr = new float[inputSize];
            
        }
    }
#ifdef ENABLE_THREADPROCESS
void mem_cp(float* img_Start, float* patch_Start, int niter, size_t n, int pW, int W, bool fill_p) {
    if(fill_p) {
        for(int i =0 ; i < niter; i++) {
            memcpy(patch_Start + i * pW , img_Start + i * W, n * sizeof(float));
        }
    }
    else {
        for(int i =0 ; i < niter; i++) {
            memcpy(img_Start + i * W, patch_Start + i * pW, n * sizeof(float)); //TODO only copy not consider average
        }
    }
}
#endif

#ifdef ENABLE_THREADPROCESS
void multithread_p_5w(float* inputBuf, float* patchBuf, std::vector<int> inputDims, int patchSize[], bool fill_p) {
    if(inputDims.size() < 5){
        std::cout<<"Error: expected inputs in 5 dims(B,N,C,H,W), but got "<< inputDims.size() <<"dims." <<std::endl;
        return;
    }
    int B = inputDims[0], N = inputDims[1], C = inputDims[2], H = inputDims[3], W = inputDims[4];
    int pH = patchSize[0], pW = patchSize[1];

    int c_thr = C;
    int b_thr = B;
    int n_thr = N;
    int h_thr = 1;
    int blockSize[] = {(int)ceil(H * 1.0 / pH), (int)ceil(W * 1.0/ pW)};
    int p_thr = blockSize[0] * blockSize[1];

    int innerH_iter_ = pH / h_thr;
    omp_set_num_threads(p_thr * c_thr * h_thr * b_thr * n_thr);
    //omp_set_num_threads(1);
    #pragma omp parallel for 
    for(int i = 0; i < p_thr * c_thr * h_thr * b_thr * n_thr; i++) {
        int innerH_iter = innerH_iter_;
        float* patch = patchBuf;

        int idx_B = i / (p_thr * c_thr * h_thr * n_thr);
        int idx_N = (i % (p_thr * c_thr * h_thr * n_thr )) / (p_thr * c_thr * h_thr );
        int idx_C = (i % (p_thr * c_thr * h_thr)) / (p_thr * h_thr);
        int idx_P = (i % (p_thr * h_thr)) / h_thr;
        int y_cor = idx_P % blockSize[1];
        int x_cor = idx_P / blockSize[1];
        int interHeight = (pH * blockSize[0] - H) / (blockSize[0] - 1);
        int interWidth = (pW * blockSize[1] - W) / (blockSize[1] - 1);
        int offset_in_image = x_cor * (pH - interHeight) * W + y_cor * (pW - interWidth);
        int offset = idx_B * H * W * C * N + idx_N * H * W * C + idx_C * H * W + offset_in_image;
        float* ptr_P = inputBuf + offset;
        
        //int lastFillHeight = (pH * blockSize[0] - H) % (blockSize[0] - 1); //TODO: need consider
        
        int h_index = i % h_thr;
        float* ptr_Start = ptr_P + (h_index * innerH_iter) * W; // calculate img thread ptr

        int patch_offset = idx_P * C * pH * pW * N * B + idx_B * C * pH * pW * N + idx_N * C * pH * pW + idx_C * pW * pH;
        float* patch_P = patch + patch_offset;
        float* patch_Start = patch_P + (i % h_thr) * innerH_iter * pW;


        mem_cp(ptr_Start, patch_Start, innerH_iter, pW, pW, W, fill_p); //copy 1920 one-time
    }
}
#endif

IBasicVSRStatus SmartPatch::generatePatch(){
#ifdef ENABLE_PERF
    auto my_tmpStartTime = Time::now();
#endif
    // no patch division
    if (!flag){
        _patchInputPtrList.push_back(_inputPtr);
        _patchOutputPtrList.push_back(_outputPtr);
        return SUCCESS;
    }

    // patch division
    // -generate patches for input data
    int inputHeight = *(_inputShape.end() - 2);
    int inputWidth = *(_inputShape.end() - 1);
    int patchSize[] = {_config.patchHeight, _config.patchWidth};
    int blockSize[] = {(int)ceil((inputHeight) * 1.0 / _config.patchHeight), (int)ceil(inputWidth * 1.0 / _config.patchWidth)};
    _config.block_h = blockSize[0], _config.block_w = blockSize[1]; 
    std::vector<int> inputDims = _inputShape; //{1,3,3,1080,1920} // from where?
    

#ifdef ENABLE_THREADPROCESS
    multithread_p_5w((float*)_inputPtr, _patchInputPtr, inputDims, patchSize, true);
    for(int i = 0; i < _config.block_h * _config.block_w; i++) {
        _patchInputPtrList.push_back((char*)(_patchInputPtr + i * (inputDims[0] * inputDims[1] * inputDims[2] * patchSize[0] * patchSize[1])));
    }
#else
    float* patchBuf = _patchInputPtr;
    std::vector<std::vector<int>> patchCoorList = calculatePatchCoordinateList(inputHeight, inputWidth, patchSize, blockSize);
    for (auto& patchCorners: patchCoorList){
        _patchInputPtrList.push_back((char*)patchBuf);
        auto patchPtr = fill_patch(patchCorners,(float*)_inputPtr, inputDims, patchBuf);
        patchBuf = patchPtr;
    }
#endif

    // -generate patches for output buffer to reserve patch output
    int inferOutHeight = inputHeight * _config.scale, inferOutWidth = inputWidth * _config.scale;
    int patchOutHeight = _config.patchHeight * _config.scale, patchOutWidth = _config.patchWidth * _config.scale;
    int outPatchSize[] = {patchOutHeight, patchOutWidth};
    float* outPatchBuf = _patchOutputPtr;
    std::vector<int> outputDims(_inputShape);
    *(outputDims.end() - 2) = inferOutHeight, *(outputDims.end() - 1) = inferOutWidth; // {1,3,3,2160,3840}

#ifdef ENABLE_THREADPROCESS
    for(int i = 0; i < _config.block_h * _config.block_w; i++) {
        _patchOutputPtrList.push_back((char*)(outPatchBuf + i * (outputDims[0] * outputDims[1] * outputDims[2] * outPatchSize[0] * outPatchSize[1])));
    }
#else
    int outBlockSize[] = {ceil(inferOutHeight*1.0/patchOutHeight), ceil(inferOutWidth*1.0/patchOutWidth)};
    std::vector<std::vector<int>> outPatchCoorList = calculatePatchCoordinateList(inferOutHeight, inferOutWidth, outPatchSize, outBlockSize);
    for (auto& patchCorners: outPatchCoorList){
        _patchOutputPtrList.push_back((char*)outPatchBuf);
        auto patchPtr = fill_patch(patchCorners,_patchOutputPtr, outputDims, outPatchBuf);
        outPatchBuf = patchPtr;
    }
#endif

    if(_patchOutputPtrList.size()!=_patchInputPtrList.size()){
        return ERROR;
    }

#ifdef ENABLE_PERF    
    auto my_tmp_duration = get_duration_ms_till_now(my_tmpStartTime);
    std::cout << "[PERF] " << "fill_patch latency: " << double_to_string(my_tmp_duration) <<"ms"<<std::endl;
#endif
    
    return SUCCESS;
    
}

IBasicVSRStatus SmartPatch::restoreImageFromPatches(){
#ifdef ENABLE_PERF
    auto my_tmpStartTime = Time::now();
#endif
    // image solution
    if(!flag){
        return SUCCESS;
    }

    // patch solution - restore to image
    int inputHeight = *(_inputShape.end() - 2), inputWidth = *(_inputShape.end() - 1);
    int inferOutHeight = inputHeight * _config.scale, inferOutWidth = inputWidth * _config.scale;
    int patchOutHeight = _config.patchHeight * _config.scale, patchOutWidth = _config.patchWidth * _config.scale;
    int outPatchSize[] = {patchOutHeight, patchOutWidth};
    std::vector<int> imgDims(_inputShape);    // 1x3x3x2160x3840 
    *(imgDims.end() - 2) = inferOutHeight, *(imgDims.end() - 1) = inferOutWidth;

#ifdef ENABLE_THREADPROCESS
    multithread_p_5w((float*)_outputPtr, (float*)_patchOutputPtrList[0], imgDims, outPatchSize, false);
#else
    int outBlockSize[] = {ceil(inferOutHeight * 1.0 / patchOutHeight), ceil(inferOutWidth * 1.0 / patchOutWidth)};
    std::vector<int> patchDims(_inputShape);  // 1x3x3x400x700 
    *(patchDims.end() - 2) = patchOutHeight, *(patchDims.end() - 1) = patchOutWidth;

    // get the patch division coordinates
    std::vector<std::vector<int>> outPatchCoorList = calculatePatchCoordinateList(inferOutHeight, inferOutWidth, outPatchSize, outBlockSize);

    // restore image according to patch pointer list and patch coordinate list
    fill_image(outPatchCoorList,_outputPtr,patchDims,imgDims,_patchOutputPtrList);
#endif

#ifdef ENABLE_PERF   
    auto my_tmp_duration = get_duration_ms_till_now(my_tmpStartTime);
    std::cout << "[PERF] " << "fill_image latency: " << double_to_string(my_tmp_duration) <<"ms"<<std::endl;
#endif 

    return SUCCESS;
}

std::vector<char*> SmartPatch::getInputPatches(){
    return _patchInputPtrList;
}
std::vector<char*> SmartPatch::getOutputPatches(){
    return _patchOutputPtrList;

}

SmartPatch::~SmartPatch(){
    if(_patchInputPtr != nullptr){
        delete[] _patchInputPtr;
        _patchInputPtr = nullptr;
    }
    if(_patchOutputPtr != nullptr){
        delete[] _patchOutputPtr;
        _patchOutputPtr = nullptr;
    }
    if(_outputPixelCount != nullptr){
        delete[] _outputPixelCount;
        _outputPixelCount = nullptr;
    }
 }