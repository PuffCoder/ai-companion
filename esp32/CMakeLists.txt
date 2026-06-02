cmake_minimum_required(VERSION 3.16)

# Default target — override with: idf.py set-target <chip>
if(NOT DEFINED IDF_TARGET)
    set(IDF_TARGET "esp32s3")
endif()

set(EXTRA_COMPONENT_DIRS "components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ai_companion)
