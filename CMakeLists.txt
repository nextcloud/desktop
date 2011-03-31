cmake_minimum_required(VERSION 2.8)
project(mirall)

find_package(Qt4 4.4.3 COMPONENTS QtCore QtGui QtXml QtNetwork QtTest REQUIRED )

macro(add_tests)
foreach( loop_var ${ARGV} )
  qt4_automoc(test${loop_var}.cpp)
  add_executable(test${loop_var} test${loop_var}.cpp)
  target_link_libraries(test${loop_var} ${QT_LIBRARIES} mirall_static)
  add_test(test${loop_var} ${CMAKE_CURRENT_BINARY_DIR}/test${loop_var} --catch_system_errors=no)
endforeach( loop_var )
endmacro(add_tests)

include(CTest)
enable_testing()

add_subdirectory(src)
add_subdirectory(test)
