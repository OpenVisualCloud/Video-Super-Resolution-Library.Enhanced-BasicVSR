# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: AI TECHNOLOGY EVALUATION LICENSE
#
cmake_minimum_required(VERSION 3.10)

option(REBUILD_IRGUARD "Enables cpu id checking" OFF)

if(REBUILD_IRGUARD)
    add_subdirectory(model_guard)
else()
    add_subdirectory(model_guard.bin)
endif()
