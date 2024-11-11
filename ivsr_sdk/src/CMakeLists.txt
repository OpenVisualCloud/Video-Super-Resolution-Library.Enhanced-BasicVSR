# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: AI TECHNOLOGY EVALUATION LICENSE
#
set(TARGET_NAME "ivsr")

# set(LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../lib)

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

add_library(${TARGET_NAME} SHARED ${HEADERS} ${SOURCES})

if(ENABLE_PERF)
	set(COMPILE_DEFINITIONS ${COMPILE_DEFINITIONS} ENABLE_PERF)
	add_definitions(-DENABLE_PERF)
endif()

if(ENABLE_LOG)
	set(COMPILE_DEFINITIONS ${COMPILE_DEFINITIONS} ENABLE_LOG)
	add_definitions(-DENABLE_LOG)
endif()

if(ENABLE_THREADPROCESS)
	set(COMPILE_DEFINITIONS ${COMPILE_DEFINITIONS} ENABLE_THREADPROCESS)
	add_definitions(-DENABLE_THREADPROCESS)
endif()

target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include/")
target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/")
target_include_directories(${TARGET_NAME} PRIVATE ${SDK_PRIVATE_HEADERS})

find_package(OpenVINO REQUIRED COMPONENTS Runtime)

target_link_libraries(${TARGET_NAME} PRIVATE openvino::runtime)

find_package(OpenMP REQUIRED)
target_link_libraries(${TARGET_NAME} PRIVATE OpenMP::OpenMP_CXX)

if (REBUILD_IRGUARD)
    target_link_libraries(${TARGET_NAME} PRIVATE irguard z)
else()
    target_link_libraries(${TARGET_NAME} PRIVATE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libirguard.a z)
endif()

