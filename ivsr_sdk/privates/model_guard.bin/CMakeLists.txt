# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: AI TECHNOLOGY EVALUATION LICENSE
#
set(TARGET_NAME "irguard")

find_package(OpenVINO REQUIRED)

message(STATUS "OpenVINO version: ${OpenVINO_VERSION}")

# Construct the original full path in the destination directory
set(original_file "${CMAKE_CURRENT_SOURCE_DIR}/libirguard.a-${OpenVINO_VERSION}")

# Construct the new desired full path with the renamed file in the destination directory
set(renamed_file "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libirguard.a")

# Ensure the copying is done before the renaming
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${original_file} ${renamed_file})
