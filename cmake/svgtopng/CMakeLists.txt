# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

# GenerateIconsUtils.cmake builds this directory as a project of its own while the client is configured,
# so it must not rely on anything the CMake files of the client define.
cmake_minimum_required(VERSION 3.16)
project(svgtopng LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Gui Svg)

add_executable(svgtopng main.cpp)
target_link_libraries(svgtopng PRIVATE Qt6::Gui Qt6::Svg)

# The generator expression keeps multi-configuration generators from adding a directory per configuration,
# so the tool is in the same place with every generator.
set_target_properties(svgtopng PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}>")
