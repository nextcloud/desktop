cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 17)

project(close_window LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(close_window main.cpp)

target_compile_definitions(close_window PRIVATE UNICODE _UNICODE)

