include(${QT_USE_FILE})

# webdav library
add_definitions("-DQWEBDAV_STATIC")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libqtwebdav)
set( qtwebdav_LIB_SRCS
     libqtwebdav/qwebdav.cpp
     libqtwebdav/qwebdav_url_info.cpp)

set( qtwebdav_HEADERS
     libqtwebdav/qwebdav.h)

qt4_wrap_cpp(qtwebdavMoc ${qtwebdav_HEADERS})

add_library(qtwebdav_static STATIC ${qtwebdavMoc} ${qtwebdav_LIB_SRCS} ${qtwebdav_HEADERS} )
target_link_libraries(qtwebdav_static ${QT_LIBRARIES})

# -------------------------------------------------------

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

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

set(mirall_HEADERS
    mirall/application.h    
    mirall/folder.h  
    mirall/folderman.h
    mirall/folderwatcher.h
    mirall/folderwizard.h
    mirall/gitfolder.h
    mirall/mirallwebdav.h
    mirall/owncloudfolder.h
    mirall/owncloudinfo.h
    mirall/owncloudsetupwizard.h
    mirall/owncloudwizard.h
    mirall/sitecopyfolder.h
    mirall/statusdialog.h
    mirall/unisonfolder.h
    mirall/theme.h
    mirall/csyncthread.h
)

IF( UNIX )
    set(mirall_SRCS ${mirall_SRCS} mirall/inotify.cpp)
    set(mirall_HEADERS ${mirall_HEADERS} mirall/inotify.h)
ENDIF(UNIX)

# Disabled the csync found check. Csync required for now.
set(mirall_SRCS
    ${mirall_SRCS}
    mirall/csyncfolder.cpp
    mirall/owncloudfolder.cpp
    mirall/csyncthread.cpp
  )
include_directories(${CSYNC_INCLUDE_DIR})

set(mirall_HEADERS
    ${mirall_HEADERS}
    mirall/csyncfolder.h
    mirall/owncloudfolder.h)

qt4_wrap_cpp(mirallMoc ${mirall_HEADERS})

add_library(mirall_static STATIC ${mirall_SRCS} ${mirall_UI_SRCS} ${mirallMoc} ${mirall_HEADERS} ${MIRALL_RC_SRC})
target_link_libraries(mirall_static ${QT_LIBRARIES})

add_executable( mirall main.cpp )
set( final_src ${qtwebdav_LIB_SRCS} ${qtwebdav_HEADERS}  ${mirall_HEADERS} ${qtwebdavMoc}
               ${MIRALL_RC_SRC} ${mirall_SRCS} ${mirall_UI_SRCS} ${mirallMoc} ${mirall_HEADERS} )

add_executable(owncloud main.cpp ${final_src})

SET(BIN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
SET_TARGET_PROPERTIES( mirall PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY} )
SET_TARGET_PROPERTIES( owncloud PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY} )

target_link_libraries(mirall mirall_static)
target_link_libraries(mirall qtwebdav_static)

target_link_libraries(owncloud ${QT_LIBRARIES} )

target_link_libraries(mirall ${CSYNC_LIBRARY})
target_link_libraries(owncloud ${CSYNC_LIBRARY})

install(TARGETS mirall
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib )
install(TARGETS owncloud 
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib )

install(FILES mirall.png DESTINATION share/icons/hicolor/48x48/apps
)

