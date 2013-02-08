include(${QT_USE_FILE})

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

QT4_ADD_RESOURCES ( MIRALL_RC_SRC ../mirall.qrc)
if ( DEFINED OEM_THEME_DIR)
    QT4_ADD_RESOURCES ( MIRALL_RC_SRC ${OEM_THEME_DIR}/theme.qrc)
    set(theme_dir ${OEM_THEME_DIR}/theme)
else()
    QT4_ADD_RESOURCES ( MIRALL_RC_SRC ../theme.qrc)
    set(theme_dir ${CMAKE_CURRENT_SOURCE_DIR}/../theme)
endif()

set(mirall_UI
mirall/folderwizardsourcepage.ui
mirall/folderwizardtargetpage.ui
mirall/folderwizardnetworkpage.ui
mirall/folderwizardowncloudpage.ui
mirall/statusdialog.ui
mirall/owncloudwizardselecttypepage.ui
mirall/owncloudsetuppage.ui
mirall/createanowncloudpage.ui
mirall/owncloudftpaccesspage.ui
mirall/owncloudwizardresultpage.ui
mirall/owncloudcredentialspage.ui
mirall/sslerrordialog.ui
mirall/proxydialog.ui
mirall/fileitemdialog.ui
)

set(3rdparty_SRC
3rdparty/qtsingleapplication/qtsingleapplication.cpp
3rdparty/qtsingleapplication/qtlocalpeer.h
3rdparty/qtsingleapplication/qtlocalpeer.cpp
3rdparty/qtsingleapplication/qtsingleapplication.h
3rdparty/qtsingleapplication/qtsinglecoreapplication.h
3rdparty/qtsingleapplication/qtsinglecoreapplication.cpp

3rdparty/qtlockedfile/qtlockedfile.h
3rdparty/qtlockedfile/qtlockedfile.cpp
)

set(3rdparty_HEADER
3rdparty/qtsingleapplication/qtlocalpeer.h
3rdparty/qtsingleapplication/qtsingleapplication.h
3rdparty/qtsingleapplication/qtsinglecoreapplication.h
)
qt4_wrap_cpp(3rdparty_MOC ${3rdparty_HEADER})

if(NOT WIN32) 
	list(APPEND 3rdparty_SRC 3rdparty/qtlockedfile/qtlockedfile_unix.cpp)
else()
	list(APPEND 3rdparty_SRC 3rdparty/qtlockedfile/qtlockedfile_win.cpp )
endif()

set(3rdparty_INC
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qtlockedfile
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qtsingleapplication
    )

qt4_wrap_ui(mirall_UI_SRCS ${mirall_UI})

set(libsync_SRCS
    mirall/folderman.cpp
    mirall/folder.cpp
    mirall/folderwatcher.cpp
    mirall/gitfolder.cpp
    mirall/syncresult.cpp
    mirall/unisonfolder.cpp
    mirall/networklocation.cpp
    mirall/mirallconfigfile.cpp
    mirall/credentialstore.cpp
    mirall/csyncfolder.cpp
    mirall/owncloudfolder.cpp
    mirall/csyncthread.cpp
    mirall/fileutils.cpp
    mirall/theme.cpp
    mirall/owncloudtheme.cpp
    mirall/miralltheme.cpp
    mirall/owncloudinfo.cpp
    mirall/logger.cpp
    mirall/utility.cpp
)
set(libsync_HEADERS 
    mirall/folderman.h
    mirall/folder.h
    mirall/folderwatcher.h
    mirall/gitfolder.h
    mirall/unisonfolder.h
    mirall/csyncfolder.h
    mirall/owncloudfolder.h
    mirall/csyncthread.h
    mirall/owncloudinfo.h
    mirall/credentialstore.h
    mirall/logger.h
)

IF( INOTIFY_FOUND )
    set(libsync_SRCS ${libsync_SRCS} mirall/inotify.cpp)
    set(libsync_SRCS ${libsync_SRCS} mirall/folderwatcher_inotify.cpp)
    set(libsync_HEADERS ${libsync_HEADERS} mirall/inotify.h)
    set(libsync_HEADERS ${libsync_HEADERS} mirall/folderwatcher_inotify.h)
ENDIF()
IF( WIN32 ) 
    set(libsync_SRCS ${libsync_SRCS} mirall/folderwatcher_win.cpp)
    set(libsync_HEADERS ${libsync_HEADERS} mirall/folderwatcher_win.h)
ENDIF()
IF( APPLE )
    set(libsync_SRCS ${libsync_SRCS} mirall/folderwatcher_mac.cpp)
ENDIF()


qt4_wrap_cpp(syncMoc ${libsync_HEADERS})

list(APPEND libsync_LINK_TARGETS
    ${QT_LIBRARIES}
    ${CSYNC_LIBRARY}
    dl
)

if(QTKEYCHAIN_FOUND)
    list(APPEND libsync_LINK_TARGETS ${QTKEYCHAIN_LIBRARY})
    include_directories(${QTKEYCHAIN_INCLUDE_DIR})
endif()

add_library(owncloudsync SHARED ${libsync_SRCS} ${syncMoc})
set_target_properties( owncloudsync PROPERTIES COMPILE_DEFINITIONS OWNCLOUD_CLIENT)
set_target_properties( owncloudsync  PROPERTIES
	VERSION ${VERSION}
	SOVERSION ${SOVERSION}
)

target_link_libraries(owncloudsync ${libsync_LINK_TARGETS} )

if ( APPLE )
    target_link_libraries(owncloudsync /System/Library/Frameworks/CoreServices.framework)
endif()

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE)
    install(TARGETS owncloudsync
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
    if(NOT WIN32)
        configure_file(${CMAKE_SOURCE_DIR}/mirall.desktop.in
                       ${APPLICATION_SHORTNAME}.desktop)
        install(FILES  ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_SHORTNAME}.desktop DESTINATION share/applications )
    endif()
else()
    install(TARGETS owncloudsync DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/MacOS)
endif()

set(mirall_SRCS
    mirall/application.cpp
    mirall/folderwizard.cpp
    mirall/statusdialog.cpp
    mirall/owncloudwizard.cpp
    mirall/owncloudsetupwizard.cpp
    mirall/updatedetector.cpp
    mirall/occinfo.cpp
    mirall/sslerrordialog.cpp
    mirall/logbrowser.cpp
    mirall/proxydialog.cpp
    mirall/fileitemdialog.cpp
)

set(mirall_HEADERS
    mirall/application.h
    mirall/folderwizard.h
    mirall/owncloudsetupwizard.h
    mirall/owncloudwizard.h
    mirall/statusdialog.h
    mirall/updatedetector.h
    mirall/sslerrordialog.h
    mirall/logbrowser.h
    mirall/proxydialog.h
    mirall/fileitemdialog.h
)

if( UNIX AND NOT APPLE)
    if(NOT DEFINED USE_INOTIFY)
        set(USE_INOTIFY ON)
    endif()
endif()

# csync is required.
include_directories(${CSYNC_INCLUDE_DIR}/csync ${CSYNC_INCLUDE_DIR} ${CMAKE_BINARY_DIR})
include_directories(${3rdparty_INC})

qt4_wrap_cpp(mirallMoc ${mirall_HEADERS})

qt4_add_translation(mirall_I18N ${TRANSLATIONS})

set( final_src
    ${mirall_HEADERS}
    ${MIRALL_RC_SRC}
    ${mirall_SRCS}
    ${mirall_UI_SRCS}
    ${mirallMoc}
    ${mirall_HEADERS}
    ${mirall_I18N}
    ${3rdparty_SRC}
    ${3rdparty_MOC}
)

# add executable icon on windows and osx
include( AddAppIconMacro )
set(ownCloud_old ${ownCloud})

kde4_add_app_icon( ownCloud "${theme_dir}/colored/${APPLICATION_SHORTNAME}-icon*.png")
list(APPEND final_src ${ownCloud})
set(ownCloud ${ownCloud_old})

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE)
    set(BIN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT WIN32)
    install(FILES ${CMAKE_SOURCE_DIR}/resources/mirall-48.png 
        DESTINATION share/icons/hicolor/48x48/apps/ RENAME mirall.png)
    install(FILES
        ${theme_dir}/colored/${APPLICATION_SHORTNAME}-icon-48.png 
        DESTINATION share/icons/hicolor/48x48/apps/ RENAME ${APPLICATION_SHORTNAME}.png)
endif(NOT WIN32)

    install(FILES ${mirall_I18N} DESTINATION share/mirall/i18n)

    # we may not add MACOSX_BUNDLE here, if not building one

    # add_executable( ${APPLICATION_EXECUTABLE} main.cpp ${final_src})
    add_executable( ${APPLICATION_EXECUTABLE} WIN32 main.cpp ${final_src})
    set_target_properties( ${APPLICATION_EXECUTABLE} PROPERTIES COMPILE_DEFINITIONS OWNCLOUD_CLIENT)

    # mirall is not built for the bundle
    add_executable( mirall WIN32 main.cpp ${final_src})

    target_link_libraries(mirall ${QT_LIBRARIES} )
    target_link_libraries(mirall owncloudsync)
    target_link_libraries(mirall ${CSYNC_LIBRARY})

    set_target_properties( mirall PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY} )
    install(TARGETS mirall
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
else()
    set(BIN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    include(DeployQt4)

    set(CMAKE_INSTALL_PREFIX ".") # Examples use /Applications. hurmpf.
    set(MACOSX_BUNDLE_ICON_FILE "ownCloud.icns")

    # we must add MACOSX_BUNDLE only if building a bundle
    add_executable( ${APPLICATION_EXECUTABLE} WIN32 MACOSX_BUNDLE main.cpp ${final_src})
    set_target_properties( ${APPLICATION_EXECUTABLE} PROPERTIES COMPILE_DEFINITIONS OWNCLOUD_CLIENT)

    #FIXME: hardcoded path
    if ( EXISTS ${CSYNC_BINARY_DIR}/modules/ocsync_owncloud.so )
        install(FILES ${CSYNC_BINARY_DIR}/modules/ocsync_owncloud.so DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Plugins)
    else()
        install(FILES /usr/local/lib/ocsync-0/ocsync_owncloud.so DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Plugins)
    endif()
    install(FILES ${mirall_I18N} DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/translations)

    list(APPEND dirs "/usr/local/lib")
endif()


set_target_properties( ${APPLICATION_EXECUTABLE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY}
)
target_link_libraries( ${APPLICATION_EXECUTABLE} ${QT_LIBRARIES} )
target_link_libraries( ${APPLICATION_EXECUTABLE} owncloudsync )
target_link_libraries( ${APPLICATION_EXECUTABLE} ${CSYNC_LIBRARY} )

install(TARGETS ${APPLICATION_EXECUTABLE}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        BUNDLE  DESTINATION "."
)

#FIXME: find a nice solution to make the second if(BUILD_OWNCLOUD_OSX_BUNDLE) unneccessary
# currently it needs to be done because the code right above needs to be executed no matter
# if building a bundle or not and the install_qt4_executable needs to be called afterwards
if(BUILD_OWNCLOUD_OSX_BUNDLE)
    install_qt4_executable(${OWNCLOUD_OSX_BUNDLE} "" "${OWNCLOUD_OSX_BUNDLE}/Contents/Plugins/ocsync_owncloud.so" ${dirs})
endif()

find_program(KRAZY2_EXECUTABLE krazy2)
if(KRAZY2_EXECUTABLE)
    # s/y k/y ALL k/ for building this target always
    add_custom_target( krazy krazy2 --check-sets c++,qt4,foss
                       ${PROJECT_SOURCE_DIR}/src/mirall/*.ui
                       ${PROJECT_SOURCE_DIR}/src/mirall/*.h
                       ${PROJECT_SOURCE_DIR}/src/mirall/*.cpp
)
endif()

