# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

find_package(Qt6 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS Svg)

add_library(docAssetsSupport STATIC
    arguments.cpp arguments.h
    capture.cpp capture.h
    captureguard.cpp captureguard.h
    offlinenetworkaccessmanager.cpp offlinenetworkaccessmanager.h
    offlinenetworkfactory.cpp offlinenetworkfactory.h
    outputdirectory.cpp outputdirectory.h
    setup.cpp setup.h
    accountwizardcontrollertestaccess.h
    catalogue.cpp catalogue.h
    capturescene.cpp capturescene.h
    exporter.cpp exporter.h
    iconexport.cpp iconexport.h
    livecapture.cpp livecapture.h
    liveprofile.cpp liveprofile.h
    sharingsource.cpp sharingsource.h
    wizardfixtures.cpp wizardfixtures.h
    fixtureimageprovider.cpp fixtureimageprovider.h
)
target_link_libraries(docAssetsSupport PUBLIC
    nextcloudCore
    nextcloudGuiSearch
    nextcloudGuiSearchplugin
    nextcloudGuiSharing
    nextcloudGuiSharingplugin
    Qt::Svg
)
target_compile_definitions(docAssetsSupport PRIVATE QT_NO_KEYWORDS)
target_include_directories(docAssetsSupport PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_SOURCE_DIR}/src")
set_target_properties(docAssetsSupport PROPERTIES AUTOMOC ON)

add_executable(DocAssetsCapture MACOSX_BUNDLE main.cpp)
target_link_libraries(DocAssetsCapture PRIVATE docAssetsSupport)
target_compile_definitions(DocAssetsCapture PRIVATE QT_NO_KEYWORDS)
set_target_properties(DocAssetsCapture PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    RUNTIME_OUTPUT_DIRECTORY "${BIN_OUTPUT_DIRECTORY}"
)

option(BUILD_DOC_ASSETS_TESTS "Build tests for the standalone documentation capture tool" OFF)
if(BUILD_DOC_ASSETS_TESTS)
    find_package(Qt6 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS Test)
    add_executable(DocAssetsCaptureTest testcapture.cpp)
    target_link_libraries(DocAssetsCaptureTest PRIVATE docAssetsSupport Qt::Test)
    target_compile_definitions(DocAssetsCaptureTest PRIVATE QT_NO_KEYWORDS DOC_ASSETS_EXECUTABLE="$<TARGET_FILE:DocAssetsCapture>")
    add_dependencies(DocAssetsCaptureTest DocAssetsCapture)
    ecm_mark_nongui_executable(DocAssetsCaptureTest)
    set_target_properties(DocAssetsCaptureTest PROPERTIES
        AUTOMOC ON
        EXCLUDE_FROM_ALL FALSE
        MACOSX_BUNDLE FALSE
        RUNTIME_OUTPUT_DIRECTORY "${BIN_OUTPUT_DIRECTORY}"
    )
    add_test(NAME DocAssetsCaptureTest COMMAND DocAssetsCaptureTest)
    set_tests_properties(DocAssetsCaptureTest PROPERTIES TIMEOUT 600)
endif()
