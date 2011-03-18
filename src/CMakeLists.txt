include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include(${QT_USE_FILE})

set(mirall_SRCS
mirall/application.cpp
mirall/fileutils.cpp
mirall/folder.cpp
mirall/folderwatcher.cpp
mirall/gitfolder.cpp
mirall/inotify.cpp
mirall/temporarydir.cpp
)
qt4_automoc(${mirall_SRCS})

add_library(mirall_static STATIC ${mirall_SRCS})

add_executable(mirall main.cpp)
target_link_libraries(mirall mirall_static)
target_link_libraries(mirall ${QT_LIBRARIES})

