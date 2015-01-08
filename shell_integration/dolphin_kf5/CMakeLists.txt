project(dolphin-owncloud)

cmake_minimum_required(VERSION 2.8)
set(QT_MIN_VERSION "5.3.0")

find_package(ECM 1.2.0 REQUIRED CONFIG)

include(FeatureSummary)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)
include(ECMInstallIcons)
include(ECMSetupVersion)

find_package(Qt5 CONFIG REQUIRED Core DBus Test Widgets)
find_package(KF5 REQUIRED Archive Bookmarks CoreAddons Config ConfigWidgets DBusAddons KIO KDELibs4Support Parts Activities)


find_package(KF5Konq  REQUIRED)

add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)

add_library(ownclouddolphinplugin MODULE ownclouddolphinplugin.cpp)
target_link_libraries(ownclouddolphinplugin KF5::Konq)
install(FILES ownclouddolphinplugin.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install(TARGETS ownclouddolphinplugin DESTINATION ${PLUGIN_INSTALL_DIR})
