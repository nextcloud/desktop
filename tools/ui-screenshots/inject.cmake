# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

if(NOT PROJECT_IS_TOP_LEVEL)
    return()
endif()
set(NEXTCLOUD_UI_SCREENSHOT_ISOLATED_CMAKE_BUILD ON CACHE INTERNAL
    "Build the standalone UI screenshot tool in an isolated CMake tree")

# CMAKE_PROJECT_TOP_LEVEL_INCLUDES runs before project() enables languages.
# Declare the standalone target after the normal source tree has configured,
# when its Qt and Nextcloud targets are available. CMake cannot create a
# subdirectory during deferred execution, so the target-only file is included.
cmake_language(EVAL CODE
    "cmake_language(DEFER DIRECTORY [[${CMAKE_SOURCE_DIR}]]
        CALL include [[${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt]])")
