project(dolphin-owncloud)

cmake_minimum_required(VERSION 2.8.12)
include(FeatureSummary)
set(QT_MIN_VERSION "5.3.0")
set(KF5_MIN_VERSION "5.16.0")
set(KDE_INSTALL_USE_QT_SYS_PATHS ON CACHE BOOL "Install the plugin in the right directory")

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

set(KDE_INSTALL_DIRS_NO_DEPRECATED TRUE)
include(KDEInstallDirs)
# Before KF5 5.54, kcoreaddons_add_plugin uses deprecated VAR PLUGIN_INSTALL_DIR
# when that is fixed and you want to remove this workaround,
# you need to _require_ the new enough kcoreaddons
set(PLUGIN_INSTALL_DIR "${KDE_INSTALL_PLUGINDIR}")
include(KDECMakeSettings)
include(KDECompilerSettings)
include(ECMMarkNonGuiExecutable)
include(GenerateExportHeader)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)

if (NOT DEFINED APPLICATION_EXECUTABLE )
  set ( APPLICATION_EXECUTABLE  "owncloud" )
endif()

#---HELPER---
set(OWNCLOUDDOLPHINHELPER ${APPLICATION_EXECUTABLE}dolphinpluginhelper)
add_library(${OWNCLOUDDOLPHINHELPER} SHARED ownclouddolphinpluginhelper.cpp)
target_link_libraries(${OWNCLOUDDOLPHINHELPER} Qt5::Network)
generate_export_header(${OWNCLOUDDOLPHINHELPER} BASE_NAME ownclouddolphinpluginhelper)
install(TARGETS ${OWNCLOUDDOLPHINHELPER} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

#---OVERLAY PLUGIN---
set(OWNCLOUDDOLPHINOVERLAYPLUGIN ${APPLICATION_EXECUTABLE}dolphinoverlayplugin)
kcoreaddons_add_plugin(${OWNCLOUDDOLPHINOVERLAYPLUGIN} INSTALL_NAMESPACE "kf5/overlayicon"
                       JSON ownclouddolphinoverlayplugin.json SOURCES ownclouddolphinoverlayplugin.cpp)
target_link_libraries(${OWNCLOUDDOLPHINOVERLAYPLUGIN} KF5::CoreAddons KF5::KIOCore KF5::KIOWidgets ${OWNCLOUDDOLPHINHELPER})

#---ACTION PLUGIN---
set(OWNCLOUDDOLPHINACTIONPLUGIN ${APPLICATION_EXECUTABLE}dolphinactionplugin)
add_library(${OWNCLOUDDOLPHINACTIONPLUGIN} MODULE ownclouddolphinactionplugin.cpp)
target_link_libraries(${OWNCLOUDDOLPHINACTIONPLUGIN} KF5::CoreAddons KF5::KIOCore KF5::KIOWidgets ${OWNCLOUDDOLPHINHELPER})
configure_file(ownclouddolphinactionplugin.desktop.in ${OWNCLOUDDOLPHINACTIONPLUGIN}.desktop ESCAPE_QUOTES @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${OWNCLOUDDOLPHINACTIONPLUGIN}.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR})
install(TARGETS ${OWNCLOUDDOLPHINACTIONPLUGIN} DESTINATION ${KDE_INSTALL_PLUGINDIR})
