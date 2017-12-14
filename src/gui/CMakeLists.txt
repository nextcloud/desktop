project(gui)
find_package(Qt5 REQUIRED COMPONENTS Widgets)
set(CMAKE_AUTOMOC TRUE)
set(CMAKE_AUTOUIC TRUE)
set(CMAKE_AUTORCC TRUE)
add_subdirectory(updater)

#TODO Move resources files
set(MIRALL_RC_SRC ../../client.qrc)
if (EXISTS "${OEM_THEME_DIR}/theme.qrc")
    list(APPEND MIRALL_RC_SRC ${OEM_THEME_DIR}/theme.qrc)
    set(theme_dir ${OEM_THEME_DIR}/theme)
else()
    list(APPEND MIRALL_RC_SRC ../../theme.qrc)
    set(theme_dir ${CMAKE_SOURCE_DIR}/theme)
endif()

set(client_UI_SRCS
    accountsettings.ui
    folderwizardsourcepage.ui
    folderwizardtargetpage.ui
    generalsettings.ui
    ignorelisteditor.ui
    networksettings.ui
    protocolwidget.ui
    issueswidget.ui
    activitywidget.ui
    synclogdialog.ui
    settingsdialog.ui
    sharedialog.ui
    sharelinkwidget.ui
    shareusergroupwidget.ui
    shareuserline.ui
    sslerrordialog.ui
    addcertificatedialog.ui
    proxyauthdialog.ui
    notificationwidget.ui
    wizard/owncloudadvancedsetuppage.ui
    wizard/owncloudconnectionmethoddialog.ui
    wizard/owncloudhttpcredspage.ui
    wizard/owncloudoauthcredspage.ui
    wizard/owncloudsetupnocredspage.ui
    wizard/owncloudwizardresultpage.ui
)

set(client_SRCS
    accountmanager.cpp
    accountsettings.cpp
    application.cpp
    folder.cpp
    folderman.cpp
    folderstatusmodel.cpp
    folderstatusdelegate.cpp
    folderwatcher.cpp
    folderwizard.cpp
    generalsettings.cpp
    ignorelisteditor.cpp
    lockwatcher.cpp
    logbrowser.cpp
    navigationpanehelper.cpp
    networksettings.cpp
    ocsjob.cpp
    ocssharejob.cpp
    ocsshareejob.cpp
    openfilemanager.cpp
    owncloudgui.cpp
    owncloudsetupwizard.cpp
    protocolwidget.cpp
    issueswidget.cpp
    activitydata.cpp
    activitylistmodel.cpp
    activitywidget.cpp
    activityitemdelegate.cpp
    selectivesyncdialog.cpp
    settingsdialog.cpp
    sharedialog.cpp
    sharelinkwidget.cpp
    sharemanager.cpp
    shareusergroupwidget.cpp
    sharee.cpp
    socketapi.cpp
    sslbutton.cpp
    sslerrordialog.cpp
    syncrunfilelog.cpp
    systray.cpp
    thumbnailjob.cpp
    quotainfo.cpp
    accountstate.cpp
    addcertificatedialog.cpp
    authenticationdialog.cpp
    proxyauthhandler.cpp
    proxyauthdialog.cpp
    synclogdialog.cpp
    tooltipupdater.cpp
    notificationwidget.cpp
    notificationconfirmjob.cpp
    servernotificationhandler.cpp
    guiutility.cpp
    elidedlabel.cpp
    creds/credentialsfactory.cpp
    creds/httpcredentialsgui.cpp
    creds/oauth.cpp
    wizard/postfixlineedit.cpp
    wizard/abstractcredswizardpage.cpp
    wizard/owncloudadvancedsetuppage.cpp
    wizard/owncloudconnectionmethoddialog.cpp
    wizard/owncloudhttpcredspage.cpp
    wizard/owncloudoauthcredspage.cpp
    wizard/owncloudsetuppage.cpp
    wizard/owncloudwizardcommon.cpp
    wizard/owncloudwizard.cpp
    wizard/owncloudwizardresultpage.cpp
)

IF(NOT NO_SHIBBOLETH)
    list(APPEND client_SRCS
        creds/shibbolethcredentials.cpp
        creds/shibboleth/shibbolethwebview.cpp
        creds/shibboleth/shibbolethuserjob.cpp
        wizard/owncloudshibbolethcredspage.cpp
    )
endif()

set(updater_SRCS
    updater/ocupdater.cpp
    updater/updateinfo.cpp
    updater/updater.cpp
)

IF( APPLE )
    list(APPEND client_SRCS cocoainitializer_mac.mm)
    list(APPEND client_SRCS settingsdialogmac.cpp)
    list(REMOVE_ITEM client_SRCS settingsdialog.cpp)
    list(APPEND client_SRCS socketapisocket_mac.mm)
    list(APPEND client_SRCS systray.mm)

    if(SPARKLE_FOUND)
       # Define this, we need to check in updater.cpp
       add_definitions( -DHAVE_SPARKLE )
       list(APPEND updater_SRCS updater/sparkleupdater_mac.mm)
   endif()
ENDIF()

IF( NOT WIN32 AND NOT APPLE )
set(client_SRCS ${client_SRCS} folderwatcher_linux.cpp)
ENDIF()
IF( WIN32 )
set(client_SRCS ${client_SRCS} folderwatcher_win.cpp)
ENDIF()
IF( APPLE )
list(APPEND client_SRCS folderwatcher_mac.cpp)
ENDIF()

set(3rdparty_SRC
    ../3rdparty/QProgressIndicator/QProgressIndicator.cpp
    ../3rdparty/qtlockedfile/qtlockedfile.cpp
    ../3rdparty/qtsingleapplication/qtlocalpeer.cpp
    ../3rdparty/qtsingleapplication/qtsingleapplication.cpp
    ../3rdparty/qtsingleapplication/qtsinglecoreapplication.cpp
   )

if (APPLE)
    list(APPEND 3rdparty_SRC
        ../3rdparty/qtmacgoodies/src/macpreferenceswindow.mm
        ../3rdparty/qtmacgoodies/src/macstandardicon.mm
        ../3rdparty/qtmacgoodies/src/macwindow.mm
        )
endif()

if(NOT WIN32)
   list(APPEND 3rdparty_SRC ../3rdparty/qtlockedfile/qtlockedfile_unix.cpp)
else()
   list(APPEND 3rdparty_SRC ../3rdparty/qtlockedfile/qtlockedfile_win.cpp )
endif()


find_package(Qt5LinguistTools)
if(Qt5LinguistTools_FOUND)
    qt5_add_translation(client_I18N ${TRANSLATIONS})
endif()

IF( WIN32 )
    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in
      ${CMAKE_CURRENT_BINARY_DIR}/version.rc
      @ONLY)
    set(client_version ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
    IF(NOT MSVC)
        set(client_manifest ${CMAKE_CURRENT_SOURCE_DIR}/manifest-mingw.rc)
    ENDIF()
ENDIF()

set( final_src
    ${MIRALL_RC_SRC}
    ${client_SRCS}
    ${client_UI_SRCS}
    ${client_version}
    ${client_manifest}
    ${guiMoc}
    ${client_I18N}
    ${3rdparty_SRC}
    ${3rdparty_MOC}
)

if(QTKEYCHAIN_FOUND OR QT5KEYCHAIN_FOUND)
    list(APPEND libsync_LINK_TARGETS ${QTKEYCHAIN_LIBRARY})
    include_directories(${QTKEYCHAIN_INCLUDE_DIR})
endif()

# add executable icon on windows and osx
include( AddAppIconMacro )
set(ownCloud_old ${ownCloud})

# For historical reasons we can not use the application_shortname
# for ownCloud but must rather set it manually.
if (NOT DEFINED APPLICATION_ICON_NAME)
    set(APPLICATION_ICON_NAME ${APPLICATION_SHORTNAME})
endif()

kde4_add_app_icon( ownCloud "${theme_dir}/colored/${APPLICATION_ICON_NAME}-*.png")
list(APPEND final_src ${ownCloud})
set(ownCloud ${ownCloud_old})



if(UNIX AND NOT APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
endif()

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE)

    if(NOT WIN32)
        file( GLOB _icons "${theme_dir}/colored/${APPLICATION_ICON_NAME}-icon-*.png" )
        foreach( _file ${_icons} )
            string( REPLACE "${theme_dir}/colored/${APPLICATION_ICON_NAME}-icon-" "" _res ${_file} )
            string( REPLACE ".png" "" _res ${_res} )
        install( FILES ${_file} RENAME ${APPLICATION_ICON_NAME}.png DESTINATION ${DATADIR}/icons/hicolor/${_res}x${_res}/apps )
        endforeach( _file )
    endif(NOT WIN32)

    install(FILES ${client_I18N} DESTINATION ${SHAREDIR}/${APPLICATION_EXECUTABLE}/i18n)

    # we may not add MACOSX_BUNDLE here, if not building one

    # add_executable( ${APPLICATION_EXECUTABLE} main.cpp ${final_src})
    add_executable( ${APPLICATION_EXECUTABLE} WIN32 main.cpp ${final_src})
else()
    # set(CMAKE_INSTALL_PREFIX ".") # Examples use /Applications. hurmpf.
    set(MACOSX_BUNDLE_ICON_FILE "ownCloud.icns")

    # we must add MACOSX_BUNDLE only if building a bundle
    add_executable( ${APPLICATION_EXECUTABLE} WIN32 MACOSX_BUNDLE main.cpp ${final_src})

    set (QM_DIR ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources/Translations)
    install(FILES ${client_I18N} DESTINATION ${QM_DIR})
    get_target_property(_qmake Qt5::qmake LOCATION)
    execute_process(COMMAND ${_qmake} -query QT_INSTALL_TRANSLATIONS
        OUTPUT_VARIABLE QT_TRANSLATIONS_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    file(GLOB qt_I18N ${QT_TRANSLATIONS_DIR}/qt_??.qm ${QT_TRANSLATIONS_DIR}/qt_??_??.qm)
    install(FILES ${qt_I18N} DESTINATION ${QM_DIR})
    file(GLOB qtbase_I18N ${QT_TRANSLATIONS_DIR}/qtbase_??.qm ${QT_TRANSLATIONS_DIR}/qt_??_??.qm)
    install(FILES ${qtbase_I18N} DESTINATION ${QM_DIR})
    file(GLOB qtkeychain_I18N ${QT_TRANSLATIONS_DIR}/qtkeychain*.qm)
    install(FILES ${qtkeychain_I18N} DESTINATION ${QM_DIR})
endif()

add_library(updater STATIC ${updater_SRCS})
target_link_libraries(updater ${synclib_NAME} Qt5::Widgets Qt5::Network Qt5::Xml)
target_include_directories(updater PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties( ${APPLICATION_EXECUTABLE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY}
)
# Only relevant for Linux? On OS X it by default properly checks in the bundle directory next to the exe
set_target_properties( ${APPLICATION_EXECUTABLE} PROPERTIES
	INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}/${APPLICATION_EXECUTABLE};${CMAKE_INSTALL_RPATH}" )

target_link_libraries( ${APPLICATION_EXECUTABLE} Qt5::Widgets Qt5::Network Qt5::Xml)
target_link_libraries( ${APPLICATION_EXECUTABLE} ${synclib_NAME} )
target_link_libraries( ${APPLICATION_EXECUTABLE} updater )
target_link_libraries( ${APPLICATION_EXECUTABLE} ${OS_SPECIFIC_LINK_LIBRARIES} )

target_include_directories(${APPLICATION_EXECUTABLE} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/3rdparty/QProgressIndicator
    ${CMAKE_SOURCE_DIR}/src/3rdparty/qtlockedfile
    ${CMAKE_SOURCE_DIR}/src/3rdparty/qtmacgoodies/src
    ${CMAKE_SOURCE_DIR}/src/3rdparty/qtsingleapplication
    ${CMAKE_CURRENT_BINARY_DIR}
)

## handle DBUS for Fdo notifications
if( UNIX AND NOT APPLE )
    find_package(Qt5 COMPONENTS DBus)
    target_link_libraries( ${APPLICATION_EXECUTABLE} Qt5::DBus)
    target_compile_definitions(${APPLICATION_EXECUTABLE} PRIVATE "USE_FDO_NOTIFICATIONS")
endif()

if (NOT NO_SHIBBOLETH)
    find_package(Qt5 COMPONENTS WebKitWidgets)
    target_link_libraries( ${APPLICATION_EXECUTABLE} Qt5::WebKitWidgets)
endif()

if(WITH_CRASHREPORTER)
    target_link_libraries( ${APPLICATION_EXECUTABLE} crashreporter-handler)
    target_include_directories(${APPLICATION_EXECUTABLE} "../3rdparty/libcrashreporter-qt/src/" )

    if(UNIX AND NOT MAC)
        find_package(Threads REQUIRED)
        target_link_libraries( ${APPLICATION_EXECUTABLE} ${CMAKE_THREAD_LIBS_INIT})
    endif()
endif()

# application.cpp still uses QDesktopServices::storageLocation
target_compile_definitions(${APPLICATION_EXECUTABLE} PRIVATE "QT_DISABLE_DEPRECATED_BEFORE=0")


install(TARGETS ${APPLICATION_EXECUTABLE}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        BUNDLE  DESTINATION "."
 )


# FIXME: The following lines are dup in src/gui and src/cmd because it needs to be done after both are installed
#FIXME: find a nice solution to make the second if(BUILD_OWNCLOUD_OSX_BUNDLE) unnecessary
# currently it needs to be done because the code right above needs to be executed no matter
# if building a bundle or not and the install_qt4_executable needs to be called afterwards
if(BUILD_OWNCLOUD_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY)
   get_target_property (QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
   install(CODE "
    message(STATUS \"Deploying (Qt) dependencies and fixing library paths...\")
    execute_process(COMMAND \"${CMAKE_SOURCE_DIR}/admin/osx/macdeployqt.py\" ${CMAKE_INSTALL_PREFIX}/${OWNCLOUD_OSX_BUNDLE} ${QT_QMAKE_EXECUTABLE})
    " COMPONENT RUNTIME)
endif()

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE AND NOT WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/mirall.desktop.in
                   ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_EXECUTABLE}.desktop)
    install(FILES  ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_EXECUTABLE}.desktop DESTINATION ${DATADIR}/applications )
endif()

