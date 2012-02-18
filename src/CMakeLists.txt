include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libqtwebdav)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include(${QT_USE_FILE})

# webdav library
add_definitions("-DQWEBDAV_STATIC")
set( qtwebdav_LIB_SRCS
     libqtwebdav/qwebdav.cpp
     libqtwebdav/qwebdav_url_info.cpp)
add_library(qtwebdav_static STATIC ${qtwebdav_LIB_SRCS})
target_link_libraries(qtwebdav_static ${QT_LIBRARIES})
qt4_automoc(${qtwebdav_LIB_SRCS})

# -------------------------------------------------------

QT4_ADD_RESOURCES ( MIRALL_RC_SRC ../mirall.qrc)

set(mirall_UI
mirall/folderwizardsourcepage.ui
mirall/folderwizardtargetpage.ui
mirall/folderwizardnetworkpage.ui
mirall/folderwizardowncloudpage.ui
mirall/statusdialog.ui
mirall/owncloudwizardselecttypepage.ui
mirall/createanowncloudpage.ui
mirall/owncloudftpaccesspage.ui
mirall/owncloudwizardresultpage.ui
mirall/owncloudcredentialspage.ui
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
mirall/sitecopyfolder.cpp
mirall/statusdialog.cpp
mirall/owncloudwizard.cpp
mirall/owncloudsetupwizard.cpp
mirall/owncloudinfo.cpp
mirall/mirallwebdav.cpp
mirall/theme.cpp
mirall/miralltheme.cpp
mirall/owncloudtheme.cpp
mirall/folderman.cpp
mirall/mirallconfigfile.cpp
)

if(CSYNC_FOUND)
  set(mirall_SRCS
    ${mirall_SRCS}
    mirall/csyncfolder.cpp
    mirall/owncloudfolder.cpp
    mirall/csyncthread.cpp
  )
  include_directories(${CSYNC_INCLUDE_DIR})
endif(CSYNC_FOUND)

qt4_automoc(${mirall_SRCS})


add_library(mirall_static STATIC ${mirall_SRCS} ${mirall_UI_SRCS})
target_link_libraries(mirall_static ${QT_LIBRARIES})

add_executable(mirall main.cpp ${MIRALL_RC_SRC})
target_link_libraries(mirall mirall_static)
target_link_libraries(mirall qtwebdav_static)

if(CSYNC_FOUND)
  target_link_libraries(mirall ${CSYNC_LIBRARY})
endif(CSYNC_FOUND)

install(TARGETS mirall
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib )
install(FILES mirall.png DESTINATION share/icons/hicolor/48x48/apps
)

