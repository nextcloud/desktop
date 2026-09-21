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
    wizardfixtures.cpp wizardfixtures.h
    fixtureaccount.cpp fixtureaccount.h
    fixtureactivitylistmodel.cpp fixtureactivitylistmodel.h
    settingsfixtures.cpp settingsfixtures.h
    fixturefolderstatusmodel.cpp fixturefolderstatusmodel.h
    fixtureimageprovider.cpp fixtureimageprovider.h
    fixturecredentials.h
    fixtureuserstatusmodel.cpp fixtureuserstatusmodel.h
    ../fakeuserstatusconnector.h
    fixturereply.cpp fixturereply.h
    fixturenetworkaccessmanager.cpp fixturenetworkaccessmanager.h
    searchresponses.cpp searchresponses.h
    searchfixtures.cpp searchfixtures.h
    sharingfixtures.cpp sharingfixtures.h
    assistantfixtures.cpp assistantfixtures.h
    ../testhelper.h
    ../assistant/fakeassistantclient.cpp ../assistant/fakeassistantclient.h
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

add_executable(DocAssetsCapture main.cpp)
target_link_libraries(DocAssetsCapture PRIVATE docAssetsSupport)
target_compile_definitions(DocAssetsCapture PRIVATE QT_NO_KEYWORDS)
ecm_mark_nongui_executable(DocAssetsCapture)
set_target_properties(DocAssetsCapture PROPERTIES
    MACOSX_BUNDLE FALSE
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
