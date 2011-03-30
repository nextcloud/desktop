include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include(${QT_USE_FILE})

qt4_automoc(testfolderwatcher.cpp)
add_executable(testfolderwatcher testfolderwatcher.cpp)
target_link_libraries(testfolderwatcher ${QT_LIBRARIES} mirall_static)

qt4_automoc(testunisonfolder.cpp)
add_executable(testunisonfolder testunisonfolder.cpp)
target_link_libraries(testunisonfolder ${QT_LIBRARIES} mirall_static)
