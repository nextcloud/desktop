
cmake_minimum_required(VERSION 2.8)
project(mirall)
set(PACKAGE "mirall")
set( CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules )
include(${CMAKE_SOURCE_DIR}/VERSION.cmake)
include(GNUInstallDirs)


find_package(Qt4 4.4.3 COMPONENTS QtCore QtGui QtXml QtNetwork QtTest REQUIRED )
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
# Uncomment this to build the ownCloud client.
# Comment this to build the original mirall client
add_definitions(-DOWNCLOUD_CLIENT)

set(TRANSLATIONS ${CMAKE_SOURCE_DIR}/translations/mirall_de.ts)

add_subdirectory(src)
# add_subdirectory(test)

install( FILES exclude.lst DESTINATION ${CMAKE_INSTALL_SYSCONFDIR})


