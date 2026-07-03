cmake_minimum_required(VERSION 3.16)

# Default target — override with: idf.py set-target <chip>
if(NOT DEFINED IDF_TARGET)
    set(IDF_TARGET "esp32s3")
endif()

set(EXTRA_COMPONENT_DIRS "components")

# 只编译 main 及其依赖闭包（避免编译废弃/未用组件，如 as5600、mic 组件）
set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ai_companion)
