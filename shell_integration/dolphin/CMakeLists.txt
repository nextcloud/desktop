project(dolphin-owncloud)

cmake_minimum_required(VERSION 3.16)

set(QT_MIN_VERSION "5.15.0")
set(KF5_MIN_VERSION "5.16.0")
set(KDE_INSTALL_USE_QT_SYS_PATHS ON CACHE BOOL "Install the plugin in the right directory")

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Network)

find_package(ECM ${KF5_MIN_VERSION} REQUIRED CONFIG)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS CoreAddons KIO)

set(KDE_INSTALL_DIRS_NO_DEPRECATED TRUE)
include(KDEInstallDirs)
# Before KF5 5.54, kcoreaddons_add_plugin uses deprecated VAR PLUGIN_INSTALL_DIR
# when that is fixed and you want to remove this workaround,
# you need to _require_ the new enough kcoreaddons
set(PLUGIN_INSTALL_DIR "${KDE_INSTALL_PLUGINDIR}")
include(KDECMakeSettings)
include(KDECompilerSettings)

add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)

#---HELPER---
set(OWNCLOUDDOLPHINHELPER ${APPLICATION_EXECUTABLE}dolphinpluginhelper)
add_library(${OWNCLOUDDOLPHINHELPER} SHARED
    ownclouddolphinpluginhelper.h
    ownclouddolphinpluginhelper.cpp)
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
configure_file(ownclouddolphinactionplugin.desktop.in ${OWNCLOUDDOLPHINACTIONPLUGIN}.desktop ESCAPE_QUOTES @ONLY)
kcoreaddons_add_plugin(${OWNCLOUDDOLPHINACTIONPLUGIN} INSTALL_NAMESPACE "kf5/kfileitemaction"
                       SOURCES ownclouddolphinactionplugin.cpp)
target_link_libraries(${OWNCLOUDDOLPHINACTIONPLUGIN} KF5::CoreAddons KF5::KIOCore KF5::KIOWidgets ${OWNCLOUDDOLPHINHELPER})
kcoreaddons_desktop_to_json(${OWNCLOUDDOLPHINACTIONPLUGIN} ${CMAKE_CURRENT_BINARY_DIR}/${OWNCLOUDDOLPHINACTIONPLUGIN}.desktop)
