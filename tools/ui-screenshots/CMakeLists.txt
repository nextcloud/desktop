# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

if(NOT NEXTCLOUD_UI_SCREENSHOT_ISOLATED_CMAKE_BUILD)
    message(FATAL_ERROR "UI screenshots may only be configured in their isolated build tree.")
endif()
if(NOT APPLE)
    message(FATAL_ERROR "The UI screenshot workflow is currently supported only on macOS.")
endif()
set(ui_screenshot_source_root "${CMAKE_CURRENT_LIST_DIR}")
set(ui_screenshot_source_dir "${ui_screenshot_source_root}/uiscreenshots")
set(ui_screenshot_binary_dir "${CMAKE_BINARY_DIR}/tools/ui-screenshots")
set(ui_screenshot_generated_dir "${ui_screenshot_binary_dir}/generated")
file(READ "${ui_screenshot_source_root}/manifest.tsv" ui_screenshot_manifest_content)
file(MAKE_DIRECTORY "${ui_screenshot_generated_dir}")
configure_file(
    "${ui_screenshot_source_dir}/uiscreenshotmanifestdata.h.in"
    "${ui_screenshot_generated_dir}/uiscreenshotmanifestdata.h"
    @ONLY)

# Keep File Provider system-domain and XPC operations out of the isolated
# process without adding a screenshot branch to production source code. The
# production settings widgets still compile unchanged against the same API.
get_target_property(ui_screenshot_nextcloud_core_sources nextcloudCore SOURCES)
set(ui_screenshot_file_provider_controller_sources ${ui_screenshot_nextcloud_core_sources})
list(FILTER ui_screenshot_file_provider_controller_sources
    INCLUDE REGEX "(^|/)macOS/fileprovidersettingscontroller_mac\\.mm$")
list(LENGTH ui_screenshot_file_provider_controller_sources ui_screenshot_file_provider_controller_source_count)
if(NOT ui_screenshot_file_provider_controller_source_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one production File Provider settings controller source.")
endif()
list(FILTER ui_screenshot_nextcloud_core_sources
    EXCLUDE REGEX "(^|/)macOS/fileprovidersettingscontroller_mac\\.mm$")
set_property(TARGET nextcloudCore PROPERTY SOURCES ${ui_screenshot_nextcloud_core_sources})
set_property(TARGET nextcloudCore APPEND PROPERTY SOURCES
    "${ui_screenshot_source_dir}/fileprovidersettingscontrollerfixture.cpp")

add_library(uiScreenshotCore STATIC
    "${ui_screenshot_source_dir}/nativescreenshotcaptureutils.cpp"
    "${ui_screenshot_source_dir}/nativescreenshotcaptureutils.h"
    "${ui_screenshot_source_dir}/nativescreenshotcontroller.cpp"
    "${ui_screenshot_source_dir}/nativescreenshotcontroller.h"
    "${ui_screenshot_source_dir}/qmlscreenshotcaptureutils.cpp"
    "${ui_screenshot_source_dir}/qmlscreenshotcaptureutils.h"
    "${ui_screenshot_source_dir}/qmlscreenshotcontroller.cpp"
    "${ui_screenshot_source_dir}/qmlscreenshotcontroller.h"
    "${ui_screenshot_source_dir}/qmlscreenshotregistrationutils.cpp"
    "${ui_screenshot_source_dir}/qmlscreenshotregistrationutils.h"
    "${ui_screenshot_source_dir}/screenshotactivitymodel.cpp"
    "${ui_screenshot_source_dir}/screenshotactivitymodel.h"
    "${ui_screenshot_source_dir}/screenshotsortedactivitylistmodel.cpp"
    "${ui_screenshot_source_dir}/screenshotsortedactivitylistmodel.h"
    "${ui_screenshot_source_dir}/screenshotsyncstatussummary.cpp"
    "${ui_screenshot_source_dir}/screenshotsyncstatussummary.h"
    "${ui_screenshot_source_dir}/screenshotsystray.cpp"
    "${ui_screenshot_source_dir}/screenshotsystray.h"
    "${ui_screenshot_source_dir}/screenshotuser.cpp"
    "${ui_screenshot_source_dir}/screenshotuser.h"
    "${ui_screenshot_source_dir}/screenshotusermodel.cpp"
    "${ui_screenshot_source_dir}/screenshotusermodel.h"
    "${ui_screenshot_source_dir}/screenshotuserstatusselectormodel.cpp"
    "${ui_screenshot_source_dir}/screenshotuserstatusselectormodel.h"
    "${ui_screenshot_source_dir}/screenshotwizardcontroller.cpp"
    "${ui_screenshot_source_dir}/screenshotwizardcontroller.h"
    "${ui_screenshot_source_dir}/uiscreenshotmanifest.cpp"
    "${ui_screenshot_source_dir}/uiscreenshotmanifest.h"
    "${ui_screenshot_source_dir}/uiscreenshotmanifestdata.h.in"
    "${ui_screenshot_source_dir}/uiscreenshotmode.cpp"
    "${ui_screenshot_source_dir}/uiscreenshotmode.h"
    "${ui_screenshot_source_dir}/uiscreenshotoutput.cpp"
    "${ui_screenshot_source_dir}/uiscreenshotoutput.h"
    "${ui_screenshot_source_dir}/uiscreenshotrunner.cpp"
    "${ui_screenshot_source_dir}/uiscreenshotrunner.h")
target_link_libraries(uiScreenshotCore PUBLIC nextcloudCore)
target_include_directories(uiScreenshotCore PUBLIC
    "${ui_screenshot_source_root}"
    "${ui_screenshot_generated_dir}"
    "${CMAKE_SOURCE_DIR}/test")
set_target_properties(uiScreenshotCore PROPERTIES AUTOMOC ON)

add_library(uiScreenshotFolderManFixture OBJECT
    "${CMAKE_SOURCE_DIR}/test/foldermantestutils.cpp"
    "${CMAKE_SOURCE_DIR}/test/foldermantestutils.h")
target_link_libraries(uiScreenshotFolderManFixture PRIVATE
    Nextcloud::sync
    Qt6::Core)
target_include_directories(uiScreenshotFolderManFixture PRIVATE "${CMAKE_SOURCE_DIR}/test")
set_target_properties(uiScreenshotFolderManFixture PROPERTIES AUTOMOC ON)

add_executable(nextcloud-ui-screenshots
    "${ui_screenshot_source_dir}/uiscreenshotmain.cpp")
target_link_libraries(nextcloud-ui-screenshots PRIVATE
    uiScreenshotCore
    uiScreenshotFolderManFixture)
set_target_properties(nextcloud-ui-screenshots PROPERTIES
    AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(BUILD_TESTING)
    find_package(Qt6 6.10.0 COMPONENTS Test REQUIRED)

    function(add_ui_screenshot_test test_name)
        add_executable(${test_name} "${ui_screenshot_source_root}/tests/${test_name}.cpp")
        target_link_libraries(${test_name} PRIVATE
            Nextcloud::sync
            uiScreenshotCore
            cmdCore
            Qt6::Test
            Qt6::Quick)
        target_include_directories(${test_name} PRIVATE "${ui_screenshot_source_root}")
        target_compile_definitions(${test_name} PRIVATE
            UI_SCREENSHOT_SOURCE_DIR="${ui_screenshot_source_root}")
        set_target_properties(${test_name} PROPERTIES
            AUTOMOC ON
            FOLDER Tests
            RUNTIME_OUTPUT_DIRECTORY "${BIN_OUTPUT_DIRECTORY}")
        add_test(NAME ${test_name}
            COMMAND ${test_name}
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    endfunction()

    add_ui_screenshot_test(UiScreenshotManifestTest)
    add_ui_screenshot_test(UiScreenshotModeTest)
    add_ui_screenshot_test(UiScreenshotOutputTest)
    add_ui_screenshot_test(UiScreenshotProductionReuseTest)
    target_link_libraries(UiScreenshotProductionReuseTest PRIVATE uiScreenshotFolderManFixture)
endif()
