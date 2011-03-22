cmake_minimum_required(VERSION 2.8)
project(mirall)

find_package(Qt4 4.4.3 COMPONENTS QtCore QtGui QtXml QtTest REQUIRED )
add_subdirectory(src)
add_subdirectory(test)

