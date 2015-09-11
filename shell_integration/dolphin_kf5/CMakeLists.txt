project(dolphin-owncloud)

cmake_minimum_required(VERSION 2.8.12)
include(FeatureSummary)
set(QT_MIN_VERSION "5.3.0")
set(KF5_MIN_VERSION "5.0.0")

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

include_directories(${CMAKE_CURRENT_BINARY_DIR})


add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)

kcoreaddons_add_plugin(ownclouddolphinplugin INSTALL_NAMESPACE "kf5/overlayicon" JSON ownclouddolphinplugin.json
            SOURCES ownclouddolphinplugin.cpp ownclouddolphinpluginhelper.cpp)
target_link_libraries(ownclouddolphinplugin Qt5::Network KF5::CoreAddons KF5::KIOCore KF5::KIOWidgets)

add_library(ownclouddolphinpluginaction MODULE ownclouddolphinpluginaction.cpp ownclouddolphinpluginhelper.cpp)
target_link_libraries(ownclouddolphinpluginaction Qt5::Network KF5::CoreAddons KF5::KIOCore KF5::KIOWidgets)
install(FILES ownclouddolphinpluginaction.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR})
install(TARGETS ownclouddolphinpluginaction DESTINATION ${KDE_INSTALL_PLUGINDIR})
