project(dolphin-owncloud)

cmake_minimum_required(VERSION 2.8.12)
include(FeatureSummary)
set(QT_MIN_VERSION "5.3.0")
set(KF5_MIN_VERSION "5.16.0")

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Network)

find_package(ECM 1.2.0 REQUIRED CONFIG)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS CoreAddons KIO)

set_package_properties(DolphinVcs PROPERTIES
    DESCRIPTION "the Dolphin plugin library"
    URL "http://dolphin.kde.org/"
    TYPE REQUIRED
    PURPOSE "Provides plugin interfaces for Dolphin."
)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)
include(ECMMarkNonGuiExecutable)
include(GenerateExportHeader)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)

#---HELPER---
add_library(ownclouddolphinpluginhelper SHARED ownclouddolphinpluginhelper.cpp)
target_link_libraries(ownclouddolphinpluginhelper Qt5::Network)
generate_export_header(ownclouddolphinpluginhelper)
install(TARGETS ownclouddolphinpluginhelper LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

#---OVERLAY PLUGIN---
kcoreaddons_add_plugin(ownclouddolphinoverlayplugin INSTALL_NAMESPACE "kf5/overlayicon"
                       JSON ownclouddolphinoverlayplugin.json SOURCES ownclouddolphinoverlayplugin.cpp)
target_link_libraries(ownclouddolphinoverlayplugin KF5::CoreAddons KF5::KIOCore KF5::KIOWidgets ownclouddolphinpluginhelper)

#---ACTION PLUGIN---
add_library(ownclouddolphinactionplugin MODULE ownclouddolphinactionplugin.cpp)
target_link_libraries(ownclouddolphinactionplugin KF5::CoreAddons KF5::KIOCore KF5::KIOWidgets ownclouddolphinpluginhelper)
install(FILES ownclouddolphinactionplugin.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR})
install(TARGETS ownclouddolphinactionplugin DESTINATION ${KDE_INSTALL_PLUGINDIR})
