cmake_minimum_required(VERSION 3.16)

# stt / llm 组件在本地 components/（自包含，自动扫描）
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(chat_test)
