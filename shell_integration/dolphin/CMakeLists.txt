cmake_minimum_required(VERSION 2.6)
project(dolphin-owncloud)
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)
find_package(LibKonq REQUIRED)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
include_directories (${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES} )
include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${LIBKONQ_INCLUDE_DIR} )

kde4_add_plugin(ownclouddolphinplugin ownclouddolphinplugin.cpp)

target_link_libraries(ownclouddolphinplugin ${KDE4_KIO_LIBS} ${LIBKONQ_LIBRARY})
install(FILES ownclouddolphinplugin.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install(TARGETS ownclouddolphinplugin DESTINATION ${PLUGIN_INSTALL_DIR})
