# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: AI TECHNOLOGY EVALUATION LICENSE
#
cmake_minimum_required(VERSION 3.10)

project(IVSR DESCRIPTION "Intel Video Super Resolution SDK")

set(OUTPUT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_FOLDER}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_FOLDER}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_FOLDER}/lib)

set(SDK_PRIVATE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/privates/include)

add_subdirectory(src)
add_subdirectory(samples)
#add_subdirectory(tools)
add_subdirectory(privates)

if(ENABLE_TEST)
    add_subdirectory(test)
endif()
