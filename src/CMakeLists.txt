#add_subdirectory(integration)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

qt_add_resources(MIRALL_RC_SRC ../mirall.qrc)
if ( IS_DIRECTORY ${OEM_THEME_DIR} )
    qt_add_resources(MIRALL_RC_SRC ${OEM_THEME_DIR}/theme.qrc)
    set(theme_dir ${OEM_THEME_DIR}/theme)
else()
    qt_add_resources(MIRALL_RC_SRC ../theme.qrc)
    set(theme_dir ${CMAKE_CURRENT_SOURCE_DIR}/../theme)
endif()

set(synclib_NAME ${APPLICATION_EXECUTABLE}sync)

set(3rdparty_SRC
3rdparty/qtsingleapplication/qtsingleapplication.cpp
3rdparty/qtsingleapplication/qtlocalpeer.cpp
3rdparty/qtsingleapplication/qtsinglecoreapplication.cpp
3rdparty/qtlockedfile/qtlockedfile.cpp
3rdparty/fancylineedit/fancylineedit.cpp
3rdparty/QProgressIndicator/QProgressIndicator.cpp
)

set(3rdparty_HEADER
3rdparty/qtsingleapplication/qtlocalpeer.h
3rdparty/qtsingleapplication/qtsingleapplication.h
3rdparty/qtsingleapplication/qtsinglecoreapplication.h
3rdparty/fancylineedit/fancylineedit.h
3rdparty/QProgressIndicator/QProgressIndicator.h
)

qt_wrap_cpp(3rdparty_MOC ${3rdparty_HEADER})

if(NOT WIN32)
	list(APPEND 3rdparty_SRC 3rdparty/qtlockedfile/qtlockedfile_unix.cpp)
else()
	list(APPEND 3rdparty_SRC 3rdparty/qtlockedfile/qtlockedfile_win.cpp )
endif()

set(3rdparty_INC
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qtlockedfile
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qtsingleapplication
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/QProgressIndicator
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fancylineedit
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/csync
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qjson
    )

set(libsync_SRCS
    mirall/folderman.cpp
    mirall/folder.cpp
    mirall/folderwatcher.cpp
    mirall/syncresult.cpp
    mirall/mirallconfigfile.cpp
    mirall/csyncthread.cpp
    mirall/owncloudpropagator.cpp
    mirall/syncjournalfilerecord.cpp
    mirall/syncjournaldb.cpp
    mirall/fileutils.cpp
    mirall/theme.cpp
    mirall/owncloudtheme.cpp
    mirall/logger.cpp
    mirall/utility.cpp
    mirall/connectionvalidator.cpp
    mirall/progressdispatcher.cpp
    mirall/mirallaccessmanager.cpp
    mirall/networkjobs.cpp
    mirall/account.cpp
    mirall/quotainfo.cpp
    creds/dummycredentials.cpp
    creds/httpcredentials.cpp
    creds/credentialsfactory.cpp
    creds/http/httpconfigfile.cpp
    creds/shibbolethcredentials.cpp
    creds/shibboleth/shibbolethaccessmanager.cpp
    creds/shibboleth/shibbolethcookiejar.cpp
    creds/shibboleth/shibbolethwebview.cpp
    creds/shibboleth/shibbolethrefresher.cpp
    creds/shibboleth/shibbolethconfigfile.cpp
    creds/credentialscommon.cpp
    3rdparty/qjson/json.cpp
)

set(libsync_HEADERS
    mirall/folderman.h
    mirall/folder.h
    mirall/folderwatcher.h
    mirall/csyncthread.h
    mirall/owncloudpropagator.h
    mirall/syncjournaldb.h
    mirall/theme.h
    mirall/owncloudtheme.h
    mirall/logger.h
    mirall/connectionvalidator.h
    mirall/progressdispatcher.h
    mirall/mirallaccessmanager.h
    mirall/networkjobs.h
    mirall/account.h
    mirall/quotainfo.h
    creds/abstractcredentials.h
    creds/dummycredentials.h
    creds/httpcredentials.h
    creds/credentialsfactory.h
    creds/http/httpconfigfile.h
    creds/shibbolethcredentials.h
    creds/shibboleth/shibbolethaccessmanager.h
    creds/shibboleth/shibbolethcookiejar.h
    creds/shibboleth/shibbolethwebview.h
    creds/shibboleth/shibbolethrefresher.h
    creds/shibboleth/shibbolethconfigfile.h
    creds/credentialscommon.h
    3rdparty/qjson/json.h
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


qt_wrap_cpp(syncMoc ${libsync_HEADERS})

# These headers are installed for libowncloudsync to be used by 3rd party apps
set(owncloudsync_HEADERS
    mirall/account.h
    mirall/csyncthread.h
    mirall/folder.h
    mirall/folderman.h
    mirall/folderwatcher.h
    mirall/mirallconfigfile.h
    mirall/networkjobs.h
    mirall/progressdispatcher.h
    mirall/syncfileitem.h
    mirall/syncjournaldb.h
    mirall/syncresult.h
)

INSTALL(
  FILES
    ${owncloudsync_HEADERS}
  DESTINATION
    ${INCLUDE_INSTALL_DIR}/owncloudsync/mirall
)

set(creds_HEADERS
    creds/abstractcredentials.h
    creds/httpcredentials.h
)

INSTALL(
  FILES
    ${creds_HEADERS}
  DESTINATION
    ${INCLUDE_INSTALL_DIR}/owncloudsync/creds
)


IF( DEFINED CSYNC_BUILD_PATH )
SET(HTTPBF_LIBRARY ${CSYNC_BUILD_PATH}/src/httpbf/libhttpbf.a)
ELSE()
FIND_LIBRARY(HTTPBF_LIBRARY NAMES httpbf HINTS $ENV{CSYNC_DIR})
ENDIF()


list(APPEND libsync_LINK_TARGETS
    ${QT_LIBRARIES}
    ${CSYNC_LIBRARY}
    ${HTTPBF_LIBRARY}
)

IF(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
   list(APPEND libsync_LINK_TARGETS
      inotify
   )
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

if(QTKEYCHAIN_FOUND)
    list(APPEND libsync_LINK_TARGETS ${QTKEYCHAIN_LIBRARY})
    include_directories(${QTKEYCHAIN_INCLUDE_DIR})
endif()

if(NEON_FOUND)
    list(APPEND libsync_LINK_TARGETS ${NEON_LIBRARIES})
    include_directories(${NEON_INCLUDE_DIRS})
endif()

add_library(${synclib_NAME} SHARED ${libsync_SRCS} ${syncMoc})

qt5_use_modules(${synclib_NAME} Widgets Network Xml WebKitWidgets Sql)

set_target_properties( ${synclib_NAME}  PROPERTIES
	VERSION ${MIRALL_VERSION}
	SOVERSION ${MIRALL_SOVERSION}
)

target_link_libraries(${synclib_NAME} ${libsync_LINK_TARGETS} )

if ( APPLE )
    target_link_libraries(${synclib_NAME} /System/Library/Frameworks/CoreServices.framework)
endif()

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE)
    install(TARGETS ${synclib_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
    if(NOT WIN32)
        configure_file(${CMAKE_SOURCE_DIR}/mirall.desktop.in
		${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_EXECUTABLE}.desktop)
        install(FILES  ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_EXECUTABLE}.desktop DESTINATION share/applications )
    endif()
else()
    install(TARGETS ${synclib_NAME} DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/MacOS)
endif()

set(mirall_UI
mirall/folderwizardsourcepage.ui
mirall/folderwizardtargetpage.ui
mirall/sslerrordialog.ui
mirall/settingsdialog.ui
mirall/generalsettings.ui
mirall/networksettings.ui
mirall/accountsettings.ui
mirall/ignorelisteditor.ui
mirall/protocolwidget.ui
wizard/owncloudsetupnocredspage.ui
wizard/owncloudhttpcredspage.ui
wizard/owncloudwizardresultpage.ui
wizard/owncloudadvancedsetuppage.ui
)

qt_wrap_ui(mirall_UI_SRCS ${mirall_UI})

set(mirall_SRCS
    mirall/application.cpp
    mirall/systray.cpp
    mirall/folderwizard.cpp
    mirall/folderstatusmodel.cpp
    mirall/protocolwidget.cpp
    wizard/owncloudwizard.cpp
    wizard/owncloudsetuppage.cpp
    wizard/owncloudhttpcredspage.cpp
    wizard/owncloudwizardresultpage.cpp
    wizard/owncloudwizardcommon.cpp
    wizard/owncloudshibbolethcredspage.cpp
    wizard/owncloudadvancedsetuppage.cpp
    mirall/owncloudsetupwizard.cpp
    mirall/updatedetector.cpp
    mirall/occinfo.cpp
    mirall/sslerrordialog.cpp
    mirall/logbrowser.cpp
    mirall/settingsdialog.cpp
    mirall/generalsettings.cpp
    mirall/networksettings.cpp
    mirall/accountsettings.cpp
    mirall/ignorelisteditor.cpp
    mirall/owncloudgui.cpp
    mirall/socketapi.cpp
)

set(mirall_HEADERS
    mirall/application.h
    mirall/systray.h
    mirall/folderwizard.h
    mirall/owncloudsetupwizard.h
    wizard/owncloudwizard.h
    wizard/owncloudsetuppage.h
    wizard/owncloudhttpcredspage.h
    wizard/owncloudwizardresultpage.h
    wizard/owncloudwizardcommon.h
    wizard/owncloudshibbolethcredspage.h
    wizard/owncloudadvancedsetuppage.h
    mirall/folderstatusmodel.h
    mirall/updatedetector.h
    mirall/sslerrordialog.h
    mirall/logbrowser.h
    mirall/settingsdialog.h
    mirall/generalsettings.h
    mirall/networksettings.h
    mirall/accountsettings.h
    mirall/ignorelisteditor.h
    mirall/protocolwidget.h
    mirall/owncloudgui.h
    mirall/socketapi.h
)

if( UNIX AND NOT APPLE)
    if(NOT DEFINED USE_INOTIFY)
        set(USE_INOTIFY ON)
    endif()
endif()

# csync is required.
include_directories(${CSYNC_INCLUDE_DIR}/csync ${CSYNC_INCLUDE_DIR} ${CSYNC_INCLUDE_DIR}/httpbf/src ${CSYNC_BUILD_PATH}/src)
include_directories(${3rdparty_INC})

qt_wrap_cpp(mirallMoc ${mirall_HEADERS})

qt_add_translation(mirall_I18N ${TRANSLATIONS})

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

# set an icon_app_name. For historical reasons we can not use the 
# application_shortname for ownCloud but must rather set it manually.
if ( EXISTS ${OEM_THEME_DIR}/OEM.cmake )
    set(ICON_APP_NAME ${APPLICATION_SHORTNAME})
else()
    set(ICON_APP_NAME "owncloud")
endif()

kde4_add_app_icon( ownCloud "${theme_dir}/colored/${ICON_APP_NAME}-icon*.png")
list(APPEND final_src ${ownCloud})
set(ownCloud ${ownCloud_old})

if (WITH_DBUS)
    set(ADDITIONAL_APP_MODULES DBus)
endif(WITH_DBUS)

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE)
    set(BIN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT WIN32)
	file( GLOB _icons "${theme_dir}/colored/${ICON_APP_NAME}-icon-*.png" )
    foreach( _file ${_icons} )
	    string( REPLACE "${theme_dir}/colored/${ICON_APP_NAME}-icon-" "" _res ${_file} )
        string( REPLACE ".png" "" _res ${_res} )
	install( FILES ${_file} RENAME ${ICON_APP_NAME}.png DESTINATION ${DATADIR}/icons/hicolor/${_res}x${_res}/apps )
    endforeach( _file )
endif(NOT WIN32)

    install(FILES ${mirall_I18N} DESTINATION share/${APPLICATION_EXECUTABLE}/i18n)

    # we may not add MACOSX_BUNDLE here, if not building one

    # add_executable( ${APPLICATION_EXECUTABLE} main.cpp ${final_src})
    add_executable( ${APPLICATION_EXECUTABLE} WIN32 main.cpp ${final_src})
    qt5_use_modules(${APPLICATION_EXECUTABLE} Widgets Network Xml WebKitWidgets Sql ${ADDITIONAL_APP_MODULES})
else()
    set(BIN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    include(DeployQt4)

    set(CMAKE_INSTALL_PREFIX ".") # Examples use /Applications. hurmpf.
    set(MACOSX_BUNDLE_ICON_FILE "ownCloud.icns")

    # we must add MACOSX_BUNDLE only if building a bundle
    add_executable( ${APPLICATION_EXECUTABLE} WIN32 MACOSX_BUNDLE main.cpp ${final_src})
    qt5_use_modules(${APPLICATION_EXECUTABLE} Widgets Network Xml WebKitWidgets Sql  ${ADDITIONAL_APP_MODULES})

    set (QM_DIR ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources/Translations)
    install(FILES ${mirall_I18N} DESTINATION ${QM_DIR})
    file(GLOB qt_I18N ${QT_TRANSLATIONS_DIR}/qt_??.qm ${QT_TRANSLATIONS_DIR}/qt_??_??.qm)
    install(FILES ${qt_I18N} DESTINATION ${QM_DIR})
    file(GLOB qtkeychain_I18N ${QT_TRANSLATIONS_DIR}/qtkeychain*.qm)
    install(FILES ${qtkeychain_I18N} DESTINATION ${QM_DIR})
endif()


set_target_properties( ${APPLICATION_EXECUTABLE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY}
)
target_link_libraries( ${APPLICATION_EXECUTABLE} ${QT_LIBRARIES} )
target_link_libraries( ${APPLICATION_EXECUTABLE} ${synclib_NAME} )
target_link_libraries( ${APPLICATION_EXECUTABLE} ${CSYNC_LIBRARY} )

install(TARGETS ${APPLICATION_EXECUTABLE}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        BUNDLE  DESTINATION "."
)

#FIXME: find a nice solution to make the second if(BUILD_OWNCLOUD_OSX_BUNDLE) unnecessary
# currently it needs to be done because the code right above needs to be executed no matter
# if building a bundle or not and the install_qt4_executable needs to be called afterwards
if(BUILD_OWNCLOUD_OSX_BUNDLE)
    install_qt4_executable(${OWNCLOUD_OSX_BUNDLE} "qtaccessiblewidgets;qsqlite")
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

set(owncloudcmd_NAME ${APPLICATION_EXECUTABLE}cmd)
set(OWNCLOUDCMD_SRC owncloudcmd/owncloudcmd.cpp)
add_executable(${owncloudcmd_NAME}  ${OWNCLOUDCMD_SRC})
qt5_use_modules(${owncloudcmd_NAME} Network Sql)
set_target_properties(${owncloudcmd_NAME} PROPERTIES
	        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY} )
target_link_libraries(${owncloudcmd_NAME} ${synclib_NAME})
target_link_libraries(${owncloudcmd_NAME} ${CSYNC_LIBRARY})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mirall)
install(TARGETS ${owncloudcmd_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
