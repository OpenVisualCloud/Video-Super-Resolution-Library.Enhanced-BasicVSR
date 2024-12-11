# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: AI TECHNOLOGY EVALUATION LICENSE
#
cmake_minimum_required(VERSION 3.10)

set (TARGET_NAME "vsr_sample")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

add_executable(vsr_sample vsr_sample.cpp)

target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include/")
target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src/include/")
target_link_libraries(${TARGET_NAME} PRIVATE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libivsr.so)

add_dependencies(vsr_sample ivsr)
find_package(OpenCV REQUIRED)
target_include_directories(${TARGET_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})

if (ENABLE_PERF)
	target_compile_definitions(${TARGET_NAME} PRIVATE ENABLE_PERF)
endif()

if (ENABLE_LOG)
	target_compile_definitions(${TARGET_NAME} PRIVATE ENABLE_LOG)
endif()

if(NOT OpenCV_FOUND)
    message(WARNING "OpenCV is disabled or not found, ${TARGET_NAME} will be built without OpenCV support")
else()
    if(UNIX AND NOT APPLE)
        # Workaround issue that rpath-link is missing for PRIVATE dependencies
        # Fixed in cmake 3.16.0 https://gitlab.kitware.com/cmake/cmake/issues/19556
        target_link_libraries(${TARGET_NAME} INTERFACE "-Wl,-rpath-link,${OpenCV_INSTALL_PATH}/lib")
    endif()
    message(STATUS "OpenCV found:")
    message(STATUS "OpenCV_VERSION=${OpenCV_VERSION}")
    target_compile_definitions(${TARGET_NAME} PRIVATE USE_OPENCV)
    target_link_libraries(${TARGET_NAME} PRIVATE opencv_core opencv_imgproc opencv_imgcodecs)
endif()

message("VSR Sample finished compile")

