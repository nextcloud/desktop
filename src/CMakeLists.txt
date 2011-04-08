include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include(${QT_USE_FILE})

set(mirall_UI
mirall/folderwizardsourcepage.ui
mirall/folderwizardtargetpage.ui
)
qt4_wrap_ui(mirall_UI_SRCS ${mirall_UI})

set(mirall_SRCS
mirall/application.cpp
mirall/fileutils.cpp
mirall/folder.cpp
mirall/folderwatcher.cpp
mirall/folderwizard.cpp
mirall/gitfolder.cpp
mirall/inotify.cpp
mirall/networklocation.cpp
mirall/temporarydir.cpp
mirall/syncresult.cpp
mirall/unisonfolder.cpp
)
if(CSYNC_FOUND)
  set(mirall_SRCS
    ${mirall_SRCS}
    mirall/csyncfolder.cpp
  )
  include_directories(${CSYNC_INCLUDE_DIR})
endif(CSYNC_FOUND)
qt4_automoc(${mirall_SRCS})


add_library(mirall_static STATIC ${mirall_SRCS} ${mirall_UI_SRCS})
target_link_libraries(mirall_static ${QT_LIBRARIES})

add_executable(mirall main.cpp)
target_link_libraries(mirall mirall_static)
if(CSYNC_FOUND)
  target_link_libraries(mirall ${CSYNC_LIBRARY})
endif(CSYNC_FOUND)

