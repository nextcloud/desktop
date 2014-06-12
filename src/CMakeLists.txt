set(CMAKE_AUTOMOC TRUE)
include(GenerateExportHeader)

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

if ( APPLE )
    list(APPEND OS_SPECIFIC_LINK_LIBRARIES
         /System/Library/Frameworks/CoreServices.framework
         /System/Library/Frameworks/Foundation.framework
         /System/Library/Frameworks/AppKit.framework
    )
endif()

IF(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    list(APPEND OS_SPECIFIC_LINK_LIBRARIES
	inotify
    )
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

if(SPARKLE_FOUND AND NOT BUILD_LIBRARIES_ONLY)
    list (APPEND OS_SPECIFIC_LINK_LIBRARIES ${SPARKLE_LIBRARY})
endif()

set(3rdparty_SRC
3rdparty/qtsingleapplication/qtsingleapplication.cpp
3rdparty/qtsingleapplication/qtlocalpeer.cpp
3rdparty/qtsingleapplication/qtsinglecoreapplication.cpp
3rdparty/qtlockedfile/qtlockedfile.cpp
3rdparty/fancylineedit/fancylineedit.cpp
3rdparty/QProgressIndicator/QProgressIndicator.cpp
)

if (APPLE)
        list(APPEND 3rdparty_SRC
        3rdparty/qtmacgoodies/src/macpreferenceswindow.mm
        3rdparty/qtmacgoodies/src/macstandardicon.mm
        )
endif()

set(3rdparty_HEADER
3rdparty/qtsingleapplication/qtlocalpeer.h
3rdparty/qtsingleapplication/qtsingleapplication.h
3rdparty/qtsingleapplication/qtsinglecoreapplication.h
3rdparty/fancylineedit/fancylineedit.h
3rdparty/QProgressIndicator/QProgressIndicator.h
)

if (APPLE)
        list(APPEND 3rdparty_HEADER
        3rdparty/qtmacgoodies/src/macpreferenceswindow.h
        3rdparty/qtmacgoodies/src/macstandardicon.h
        )
endif()

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
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qjson
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qtmacgoodies/src
    )

set(libsync_SRCS
    mirall/folderman.cpp
    mirall/folder.cpp
    mirall/folderwatcher.cpp
    mirall/syncresult.cpp
    mirall/mirallconfigfile.cpp
    mirall/syncengine.cpp
    mirall/owncloudpropagator.cpp
    mirall/propagatorjobs.cpp
    mirall/propagator_qnam.cpp
    mirall/propagator_legacy.cpp
    mirall/syncjournalfilerecord.cpp
    mirall/syncjournaldb.cpp
    mirall/theme.cpp
    mirall/owncloudtheme.cpp
    mirall/logger.cpp
    mirall/utility.cpp
    mirall/filesystem.cpp
    mirall/connectionvalidator.cpp
    mirall/progressdispatcher.cpp
    mirall/mirallaccessmanager.cpp
    mirall/networkjobs.cpp
    mirall/account.cpp
    mirall/quotainfo.cpp
    mirall/clientproxy.cpp
    mirall/syncrunfilelog.cpp
    mirall/cookiejar.cpp
    creds/dummycredentials.cpp
    creds/abstractcredentials.cpp
    creds/credentialsfactory.cpp
    creds/http/httpconfigfile.cpp
    creds/credentialscommon.cpp
    3rdparty/qjson/json.cpp
)
if(TOKEN_AUTH_ONLY)
	set (libsync_SRCS
		${libsync_SRCS}
		creds/tokencredentials.cpp
	)
else()
    set (libsync_SRCS
        ${libsync_SRCS}
        creds/httpcredentials.cpp
        creds/shibbolethcredentials.cpp
        creds/shibboleth/shibbolethwebview.cpp
        creds/shibboleth/shibbolethrefresher.cpp
        creds/shibboleth/authenticationdialog.cpp
        creds/shibboleth/shibbolethuserjob.cpp
    )
endif()

IF( NOT WIN32 AND NOT APPLE )
    set(libsync_SRCS ${libsync_SRCS} mirall/folderwatcher_linux.cpp)
ENDIF()
IF( WIN32 )
    set(libsync_SRCS ${libsync_SRCS} mirall/folderwatcher_win.cpp)
ENDIF()
IF( APPLE )
    list(APPEND libsync_SRCS mirall/folderwatcher_mac.cpp)
ENDIF()


# These headers are installed for libowncloudsync to be used by 3rd party apps
set(owncloudsync_HEADERS
    mirall/account.h
    mirall/syncengine.h
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

set(creds_HEADERS
    creds/abstractcredentials.h
    creds/httpcredentials.h
)

IF (NOT APPLE)
    INSTALL(
        FILES ${owncloudsync_HEADERS}
	DESTINATION ${INCLUDE_INSTALL_DIR}/${synclib_NAME}/mirall
    )
    INSTALL(
        FILES ${creds_HEADERS}
	DESTINATION ${INCLUDE_INSTALL_DIR}/${synclib_NAME}/creds
    )
ENDIF(NOT APPLE)

list(APPEND libsync_LINK_TARGETS
    ${QT_LIBRARIES}
    ocsync
    httpbf
    ${OS_SPECIFIC_LINK_LIBRARIES}
)

if(QTKEYCHAIN_FOUND OR QT5KEYCHAIN_FOUND)
    list(APPEND libsync_LINK_TARGETS ${QTKEYCHAIN_LIBRARY})
    include_directories(${QTKEYCHAIN_INCLUDE_DIR})
endif()

if(NEON_FOUND)
    list(APPEND libsync_LINK_TARGETS ${NEON_LIBRARIES})
    include_directories(${NEON_INCLUDE_DIRS})

    if(NEON_WITH_LFS)
        add_definitions(-DNE_LFS)
    endif()

endif()

add_library(${synclib_NAME} SHARED ${libsync_SRCS} ${syncMoc})
GENERATE_EXPORT_HEADER( ${synclib_NAME}
	BASE_NAME ${synclib_NAME}
	EXPORT_MACRO_NAME OWNCLOUDSYNC_EXPORT
        EXPORT_FILE_NAME owncloudlib.h
	STATIC_DEFINE OWNCLOUD_BUILT_AS_STATIC
)


if(TOKEN_AUTH_ONLY)
    qt5_use_modules(${synclib_NAME} Network Xml Sql)
else()
    qt5_use_modules(${synclib_NAME} Widgets Network Xml WebKitWidgets Sql)
endif()

set_target_properties( ${synclib_NAME}  PROPERTIES
	VERSION ${MIRALL_VERSION}
	SOVERSION ${MIRALL_SOVERSION}
)
set_target_properties( ${synclib_NAME} PROPERTIES
	INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}/${APPLICATION_EXECUTABLE}" )

target_link_libraries(${synclib_NAME} ${libsync_LINK_TARGETS} )

if(BUILD_LIBRARIES_ONLY)
    #add_library(${synclib_NAME}_static STATIC ${libsync_SRCS} ${syncMoc})
    #qt5_use_modules(${synclib_NAME}_static Widgets Network Xml Sql)

    #set_target_properties( ${synclib_NAME}_static  PROPERTIES
    #    VERSION ${MIRALL_VERSION}
    #    SOVERSION ${MIRALL_SOVERSION}
    #)

    #target_link_libraries(${synclib_NAME}_static ${libsync_LINK_TARGETS} )
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
    if (SPARKLE_FOUND)
        install(DIRECTORY "${SPARKLE_LIBRARY}"
	        DESTINATION "${OWNCLOUD_OSX_BUNDLE}/Contents/Frameworks")
    endif (SPARKLE_FOUND)

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
    wizard/abstractcredswizardpage.cpp
    wizard/owncloudwizardresultpage.cpp
    wizard/owncloudwizardcommon.cpp
    wizard/owncloudshibbolethcredspage.cpp
    wizard/owncloudadvancedsetuppage.cpp
    mirall/owncloudsetupwizard.cpp
    mirall/sslerrordialog.cpp
    mirall/logbrowser.cpp
    mirall/settingsdialog.cpp
    mirall/generalsettings.cpp
    mirall/networksettings.cpp
    mirall/accountsettings.cpp
    mirall/ignorelisteditor.cpp
    mirall/owncloudgui.cpp
    mirall/socketapi.cpp
    mirall/sslbutton.cpp
)



set(mirall_HEADERS
    mirall/application.h
    mirall/systray.h
    mirall/folderwizard.h
    mirall/owncloudsetupwizard.h
    wizard/owncloudwizard.h
    wizard/owncloudsetuppage.h
    wizard/owncloudhttpcredspage.h
    wizard/abstractcredswizardpage.h
    wizard/owncloudwizardresultpage.h
    wizard/owncloudwizardcommon.h
    wizard/owncloudshibbolethcredspage.h
    wizard/owncloudadvancedsetuppage.h
    mirall/folderstatusmodel.h
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
    mirall/sslbutton.h
)

set(updater_SRCS
    updater/updateinfo.cpp
    updater/updater.cpp
    updater/ocupdater.cpp
)

set(updater_HEADERS
    updater/updater.h
    updater/ocupdater.h
)

IF( APPLE )
    list(APPEND mirall_SRCS mirall/cocoainitializer_mac.mm)
    list(APPEND mirall_HEADERS mirall/cocoainitializer.h)

    list(APPEND mirall_SRCS mirall/settingsdialogmac.cpp)
    list(APPEND mirall_HEADERS mirall/settingsdialogmac.h)

    if(SPARKLE_FOUND)
       # Define this, we need to check in updater.cpp
       add_definitions( -DHAVE_SPARKLE )
       list(APPEND updater_SRCS updater/sparkleupdater_mac.mm)
       list(APPEND updater_HEADERS updater/sparkleupdater.h)
   endif()
ENDIF()

# csync is required.
include_directories(../csync/src ../csync/src/httpbf/src ${CMAKE_CURRENT_BINARY_DIR}/../csync ${CMAKE_CURRENT_BINARY_DIR}/../csync/src )
include_directories(${3rdparty_INC})

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

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY)
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
elseif(NOT BUILD_LIBRARIES_ONLY)
    set(BIN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    if (Qt5Core_FOUND)
        include(DeployQt5)
    else(Qt5Core_FOUND)
        include(DeployQt4)
    endif(Qt5Core_FOUND)

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

if(NOT BUILD_LIBRARIES_ONLY)
  add_library(updater STATIC ${updater_SRCS} ${updaterMoc})
  target_link_libraries(updater ${synclib_NAME})
  qt5_use_modules(updater Widgets Network Xml)

  set_target_properties( ${APPLICATION_EXECUTABLE} PROPERTIES
          RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY}
  )
  set_target_properties( ${APPLICATION_EXECUTABLE} PROPERTIES
	  INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}/${APPLICATION_EXECUTABLE}" )

  target_link_libraries( ${APPLICATION_EXECUTABLE} ${QT_LIBRARIES} )
  target_link_libraries( ${APPLICATION_EXECUTABLE} ${synclib_NAME} )
  target_link_libraries( ${APPLICATION_EXECUTABLE} updater )
  target_link_libraries( ${APPLICATION_EXECUTABLE} ${OS_SPECIFIC_LINK_LIBRARIES} )

  install(TARGETS ${APPLICATION_EXECUTABLE}
          RUNTIME DESTINATION bin
          LIBRARY DESTINATION lib
          ARCHIVE DESTINATION lib
          BUNDLE  DESTINATION "."
 )
endif()


#FIXME: find a nice solution to make the second if(BUILD_OWNCLOUD_OSX_BUNDLE) unnecessary
# currently it needs to be done because the code right above needs to be executed no matter
# if building a bundle or not and the install_qt4_executable needs to be called afterwards
if(BUILD_OWNCLOUD_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY)
    if(Qt5Core_FOUND)
        install_qt5_executable(${OWNCLOUD_OSX_BUNDLE} "qtaccessiblewidgets;qsqlite;qcocoa")
    else(Qt5Core_FOUND)
        install_qt4_executable(${OWNCLOUD_OSX_BUNDLE} "qtaccessiblewidgets;qsqlite")
    endif(Qt5Core_FOUND)
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
set(OWNCLOUDCMD_SRC owncloudcmd/simplesslerrorhandler.cpp owncloudcmd/owncloudcmd.cpp)
if(NOT BUILD_LIBRARIES_ONLY)

   add_executable(${owncloudcmd_NAME}  ${OWNCLOUDCMD_SRC})
	qt5_use_modules(${owncloudcmd_NAME} Network Sql)
	set_target_properties(${owncloudcmd_NAME} PROPERTIES
	        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY} )
        set_target_properties(${owncloudcmd_NAME} PROPERTIES
		INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}/${APPLICATION_EXECUTABLE}" )

	target_link_libraries(${owncloudcmd_NAME} ${synclib_NAME})
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mirall)
if(BUILD_OWNCLOUD_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY)
    install(TARGETS ${owncloudcmd_NAME} DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/MacOS)
    if (SPARKLE_FOUND)
        install(FILES ${CMAKE_SOURCE_DIR}/admin/osx/deny_autoupdate_com.owncloud.desktopclient.plist
                DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources)
        install(FILES ${CMAKE_SOURCE_DIR}/admin/osx/sparkle/dsa_pub.pem
                DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources)
    endif()
elseif(NOT BUILD_LIBRARIES_ONLY)
    install(TARGETS ${owncloudcmd_NAME}
	    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
