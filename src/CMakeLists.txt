include(${QT_USE_FILE})

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

QT4_ADD_RESOURCES ( MIRALL_RC_SRC ../mirall.qrc)
if ( IS_DIRECTORY ${OEM_THEME_DIR} )
    QT4_ADD_RESOURCES ( MIRALL_RC_SRC ${OEM_THEME_DIR}/theme.qrc)
    set(theme_dir ${OEM_THEME_DIR}/theme)
else()
    QT4_ADD_RESOURCES ( MIRALL_RC_SRC ../theme.qrc)
    set(theme_dir ${CMAKE_CURRENT_SOURCE_DIR}/../theme)
endif()

set(synclib_NAME ${APPLICATION_SHORTNAME}sync)

set(mirall_UI
mirall/folderwizardsourcepage.ui
mirall/folderwizardtargetpage.ui
mirall/sslerrordialog.ui
mirall/settingsdialog.ui
mirall/generalsettings.ui
mirall/networksettings.ui
mirall/accountsettings.ui
mirall/ignorelisteditor.ui
mirall/fileitemdialog.ui
mirall/owncloudsetupnocredspage.ui
mirall/owncloudhttpcredspage.ui
mirall/owncloudwizardresultpage.ui
mirall/owncloudshibbolethcredspage.ui
)

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
qt4_wrap_cpp(3rdparty_MOC ${3rdparty_HEADER})

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
    )

qt4_wrap_ui(mirall_UI_SRCS ${mirall_UI})

set(libsync_SRCS
    mirall/folderman.cpp
    mirall/folder.cpp
    mirall/folderwatcher.cpp
    mirall/syncresult.cpp
    mirall/networklocation.cpp
    mirall/mirallconfigfile.cpp
    mirall/csyncthread.cpp
    mirall/fileutils.cpp
    mirall/theme.cpp
    mirall/owncloudtheme.cpp
    mirall/owncloudinfo.cpp
    mirall/logger.cpp
    mirall/utility.cpp
    mirall/connectionvalidator.cpp
    mirall/progressdispatcher.cpp
    mirall/creds/dummycredentials.cpp
    mirall/creds/httpcredentials.cpp
    mirall/creds/credentialsfactory.cpp
    mirall/creds/http/credentialstore.cpp
    mirall/creds/http/httpconfigfile.cpp
    mirall/creds/shibbolethcredentials.cpp
    mirall/creds/shibboleth/shibbolethaccessmanager.cpp
    mirall/creds/shibboleth/shibbolethcookiejar.cpp
    mirall/creds/shibboleth/shibbolethwebview.cpp
)

set(libsync_HEADERS
    mirall/folderman.h
    mirall/folder.h
    mirall/folderwatcher.h
    mirall/csyncthread.h
    mirall/theme.h
    mirall/owncloudtheme.h
    mirall/owncloudinfo.h
    mirall/logger.h
    mirall/connectionvalidator.h
    mirall/progressdispatcher.h
    mirall/creds/abstractcredentials.h
    mirall/creds/dummycredentials.h
    mirall/creds/httpcredentials.h
    mirall/creds/credentialsfactory.h
    mirall/creds/http/credentialstore.h
    mirall/creds/http/httpconfigfile.h
    mirall/creds/shibbolethcredentials.h
    mirall/creds/shibboleth/shibbolethaccessmanager.h
    mirall/creds/shibboleth/shibbolethcookiejar.h
    mirall/creds/shibboleth/shibbolethwebview.h
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

add_library(${synclib_NAME} SHARED ${libsync_SRCS} ${syncMoc})
set_target_properties( ${synclib_NAME}  PROPERTIES
	VERSION ${VERSION}
	SOVERSION ${SOVERSION}
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
		${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_SHORTNAME}.desktop)
        install(FILES  ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_SHORTNAME}.desktop DESTINATION share/applications )
    endif()
else()
    install(TARGETS ${synclib_NAME} DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/MacOS)
endif()

set(mirall_SRCS
    mirall/application.cpp
    mirall/systray.cpp
    mirall/folderwizard.cpp
    mirall/folderstatusmodel.cpp
    mirall/wizard/owncloudwizard.cpp
    mirall/wizard/owncloudsetuppage.cpp
    mirall/wizard/owncloudhttpcredspage.cpp
    mirall/wizard/owncloudwizardresultpage.cpp
    mirall/wizard/owncloudwizardcommon.cpp
    mirall/wizard/owncloudshibbolethcredspage.cpp
    mirall/owncloudsetupwizard.cpp
    mirall/updatedetector.cpp
    mirall/occinfo.cpp
    mirall/sslerrordialog.cpp
    mirall/logbrowser.cpp
    mirall/fileitemdialog.cpp
    mirall/settingsdialog.cpp
    mirall/generalsettings.cpp
    mirall/networksettings.cpp
    mirall/accountsettings.cpp
    mirall/ignorelisteditor.cpp
)

set(mirall_HEADERS
    mirall/application.h
    mirall/systray.h
    mirall/folderwizard.h
    mirall/owncloudsetupwizard.h
    mirall/wizard/owncloudwizard.h
    mirall/wizard/owncloudsetuppage.h
    mirall/wizard/owncloudhttpcredspage.h
    mirall/wizard/owncloudwizardresultpage.h
    mirall/wizard/owncloudwizardcommon.h
    mirall/wizard/owncloudshibbolethcredspage.h
    mirall/folderstatusmodel.h
    mirall/updatedetector.h
    mirall/sslerrordialog.h
    mirall/logbrowser.h
    mirall/fileitemdialog.h
    mirall/settingsdialog.h
    mirall/generalsettings.h
    mirall/networksettings.h
    mirall/accountsettings.h
    mirall/ignorelisteditor.h
)

if( UNIX AND NOT APPLE)
    if(NOT DEFINED USE_INOTIFY)
        set(USE_INOTIFY ON)
    endif()
endif()

# csync is required.
include_directories(${CSYNC_INCLUDE_DIR}/csync ${CSYNC_INCLUDE_DIR} ${CSYNC_BUILD_PATH}/src)
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
    file( GLOB _icons "${theme_dir}/colored/${APPLICATION_SHORTNAME}-icon-*.png" )
    foreach( _file ${_icons} )
        string( REPLACE "${theme_dir}/colored/${APPLICATION_SHORTNAME}-icon-" "" _res ${_file} )
        string( REPLACE ".png" "" _res ${_res} )
        install( FILES ${_file} RENAME ${APPLICATION_SHORTNAME}.png DESTINATION
${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_res}x${_res}/apps )
    endforeach( _file )
endif(NOT WIN32)

    install(FILES ${mirall_I18N} DESTINATION share/${APPLICATION_SHORTNAME}/i18n)

    # we may not add MACOSX_BUNDLE here, if not building one

    # add_executable( ${APPLICATION_EXECUTABLE} main.cpp ${final_src})
    add_executable( ${APPLICATION_EXECUTABLE} WIN32 main.cpp ${final_src})

else()
    set(BIN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    include(DeployQt4)

    set(CMAKE_INSTALL_PREFIX ".") # Examples use /Applications. hurmpf.
    set(MACOSX_BUNDLE_ICON_FILE "ownCloud.icns")

    # we must add MACOSX_BUNDLE only if building a bundle
    add_executable( ${APPLICATION_EXECUTABLE} WIN32 MACOSX_BUNDLE main.cpp ${final_src})

    #FIXME: hardcoded path
    if ( EXISTS ${CSYNC_BINARY_DIR}/modules/ocsync_owncloud.so )
        install(FILES ${CSYNC_BINARY_DIR}/modules/ocsync_owncloud.so DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Plugins)
    else()
        install(FILES /usr/local/lib/ocsync-0/ocsync_owncloud.so DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Plugins)
    endif()

    set (QM_DIR ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources/Translations)
    install(FILES ${mirall_I18N} DESTINATION ${QM_DIR})
    file(GLOB qt_I18N ${QT_TRANSLATIONS_DIR}/qt_??.qm ${QT_TRANSLATIONS_DIR}/qt_??_??.qm)
    install(FILES ${qt_I18N} DESTINATION ${QM_DIR})
    file(GLOB qtkeychain_I18N ${QT_TRANSLATIONS_DIR}/qtkeychain*.qm)
    install(FILES ${qtkeychain_I18N} DESTINATION ${QM_DIR})
    list(APPEND dirs "/usr/local/lib")
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

