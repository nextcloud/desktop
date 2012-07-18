
cmake_minimum_required(VERSION 2.6)
project(mirall)
set(PACKAGE "mirall")
set( CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules )

include(${CMAKE_SOURCE_DIR}/VERSION.cmake)
configure_file( ${CMAKE_SOURCE_DIR}/src/mirall/version.h.in ${CMAKE_SOURCE_DIR}/src/mirall/version.h )

include(GNUInstallDirs)
## stupid, we should upstream this
if("${CMAKE_INSTALL_PREFIX}" EQUAL "/usr")
    set(CMAKE_INSTALL_SYSCONFDIR "/etc")
endif()

if(NOT DEFINED BUILD_OWNCLOUD_OSX_BUNDLE OR NOT APPLE)
    set(BUILD_OWNCLOUD_OSX_BUNDLE OFF)
elseif(BUILD_OWNCLOUD_OSX_BUNDLE)
    set(OWNCLOUD_OSX_BUNDLE "owncloud.app")
endif()

find_package(Qt4 4.6.0 COMPONENTS QtCore QtGui QtXml QtNetwork QtTest REQUIRED )
find_package(Csync)

if(CSYNC_FOUND)
  add_definitions(-DWITH_CSYNC)
endif(CSYNC_FOUND)

macro(add_tests)
foreach( loop_var ${ARGV} )
  qt4_automoc(test${loop_var}.cpp)
  add_executable(test${loop_var} test${loop_var}.cpp)
  target_link_libraries(test${loop_var} ${QT_LIBRARIES} mirall_static)
  add_test(test${loop_var} ${CMAKE_CURRENT_BINARY_DIR}/test${loop_var} --catch_system_errors=no)
endforeach( loop_var )
endmacro(add_tests)

set(CPACK_SOURCE_IGNORE_FILES
  # hidden files
  "/\\\\..+$"
  # temporary files
  "\\\\.swp$"
  # backup files
  "~$"
  # others
  "\\\\.#"
  "/#"
  "/build/"
  "/_build/"
  # used before
  "\\\\.o$"
  "\\\\.lo$"
  "\\\\.la$"
  "Makefile\\\\.in$"
)

include(OwnCloudCPack.cmake)

include(CTest)
enable_testing()

#
# This cmake builds two targets (aka apps), mirall and owncloud. For the owncloud
# target, OWNCLOUD_CLIENT needs to be a compile flag. It is set in src/CMakeLists.txt
# but if that fails because cmake is too old, uncomment this here if you want to build
# owncloud.
# add_definitions(-DOWNCLOUD_CLIENT)

# Handle Translations, pick all mirall_* files from trans directory.
file( GLOB TRANS_FILES ${CMAKE_SOURCE_DIR}/translations/mirall_*.ts)
set(TRANSLATIONS ${TRANS_FILES})

add_subdirectory(src)
if(UNIT_TESTING)
    add_subdirectory(test)
endif(UNIT_TESTING)

if(BUILD_OWNCLOUD_OSX_BUNDLE)
    install( FILES exclude.lst DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources )
else()
    install( FILES exclude.lst DESTINATION ${CMAKE_INSTALL_SYSCONFDIR} )
endif()

