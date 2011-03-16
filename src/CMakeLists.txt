include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include(${QT_USE_FILE})

set(mirall_SRCS
mirall/application.cpp
mirall/folder.cpp
mirall/gitfolder.cpp
mirall/folderwatcher.cpp
mirall/inotify.cpp
main.cpp)

qt4_automoc(${mirall_SRCS})

add_executable(mirall ${mirall_SRCS})
target_link_libraries(mirall ${QT_LIBRARIES})

