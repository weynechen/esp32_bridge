# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# IDF路径通常从环境变量获取
set(IDF_PATH $ENV{IDF_PATH})

# 包含ESP-IDF的项目构建系统
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_dongle) 