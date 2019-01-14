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
    clientproxy.cpp
    connectionvalidator.cpp
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
        add_definitions(-DHAVE_SPARKLE)
        list(APPEND updater_SRCS updater/sparkleupdater_mac.mm updater/sparkleupdater.h)
        list(APPEND updater_DEPS ${SPARKLE_LIBRARY})

        # Sparkle.framework is installed from here because macdeployqt's CopyFramework breaks on this bundle
        # as its logic is tightly tailored around Qt5 frameworks
        install(DIRECTORY "${SPARKLE_LIBRARY}"
                DESTINATION "${OWNCLOUD_OSX_BUNDLE}/Contents/Frameworks" USE_SOURCE_PERMISSIONS)

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

# UPSTREAM our ECMAddAppIcon.cmake then require that version here
# find_package(ECM 1.7.0 REQUIRED NO_MODULE)
# list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(ECMAddAppIcon)

file(GLOB_RECURSE OWNCLOUD_ICONS "${theme_dir}/colored/*-${APPLICATION_ICON_NAME}-icon*")
if(APPLE)
    file(GLOB_RECURSE OWNCLOUD_SIDEBAR_ICONS "${theme_dir}/colored/*-${APPLICATION_ICON_NAME}-sidebar*")
    MESSAGE(STATUS "OWNCLOUD_SIDEBAR_ICONS: ${APPLICATION_ICON_NAME}: ${OWNCLOUD_SIDEBAR_ICONS}")
endif()
ecm_add_app_icon(final_src ICONS "${OWNCLOUD_ICONS}" SIDEBAR_ICONS "${OWNCLOUD_SIDEBAR_ICONS}" OUTFILE_BASENAME "${APPLICATION_ICON_NAME}")

if(UNIX AND NOT APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
endif()

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE)
    if(NOT WIN32)
        file(GLOB _icons "${theme_dir}/colored/*-${APPLICATION_ICON_NAME}-icon.png")
        foreach(_file ${_icons})
            string(REPLACE "${theme_dir}/colored/" "" _res ${_file})
            string(REPLACE "-${APPLICATION_ICON_NAME}-icon.png" "" _res ${_res})
            install(FILES ${_file} RENAME ${APPLICATION_ICON_NAME}.png DESTINATION ${DATADIR}/icons/hicolor/${_res}x${_res}/apps)
        endforeach(_file)
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
target_link_libraries(updater ${synclib_NAME} ${updater_DEPS} Qt5::Widgets Qt5::Network Qt5::Xml)
target_include_directories(updater PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties( ${APPLICATION_EXECUTABLE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY}
)

target_link_libraries( ${APPLICATION_EXECUTABLE} Qt5::Widgets Qt5::Network Qt5::Xml)
target_link_libraries( ${APPLICATION_EXECUTABLE} ${synclib_NAME} )
target_link_libraries( ${APPLICATION_EXECUTABLE} updater )
target_link_libraries( ${APPLICATION_EXECUTABLE} ${OS_SPECIFIC_LINK_LIBRARIES} )

target_include_directories(${APPLICATION_EXECUTABLE} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/3rdparty/QProgressIndicator
    ${CMAKE_SOURCE_DIR}/src/3rdparty/qtlockedfile
    ${CMAKE_SOURCE_DIR}/src/3rdparty/qtsingleapplication
    ${CMAKE_CURRENT_BINARY_DIR}
)

if (APPLE)
    target_include_directories(${APPLICATION_EXECUTABLE} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/3rdparty/qtmacgoodies/src
    )
    # We want to access Cocoa specific structures in the code above
    # and need the platform plugin interface for that - which is private.
    target_link_libraries(${APPLICATION_EXECUTABLE} Qt5::GuiPrivate) # not actually a library. Will pull in includes
endif()

## handle DBUS for Fdo notifications
if( UNIX AND NOT APPLE )
    find_package(Qt5 COMPONENTS DBus)
    target_link_libraries( ${APPLICATION_EXECUTABLE} Qt5::DBus)
    target_compile_definitions(${APPLICATION_EXECUTABLE} PRIVATE "USE_FDO_NOTIFICATIONS")
endif()

if (APPLE)
    find_package(Qt5 COMPONENTS MacExtras)
    target_link_libraries( ${APPLICATION_EXECUTABLE} Qt5::MacExtras)
endif()

if (NOT NO_SHIBBOLETH)
    find_package(Qt5 COMPONENTS WebKitWidgets)
    target_link_libraries( ${APPLICATION_EXECUTABLE} Qt5::WebKitWidgets)
endif()

if(WITH_CRASHREPORTER)
    target_link_libraries(${APPLICATION_EXECUTABLE} crashreporter-handler)

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

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE AND NOT WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/mirall.desktop.in
                   ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_EXECUTABLE}.desktop)
    install(FILES  ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_EXECUTABLE}.desktop DESTINATION ${DATADIR}/applications )

    configure_file(owncloud.xml.in ${APPLICATION_EXECUTABLE}.xml)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_EXECUTABLE}.xml DESTINATION ${DATADIR}/mime/packages )

    find_package(ECM 1.2.0 CONFIG)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    find_package(SharedMimeInfo)
    if(SharedMimeInfo_FOUND)
        update_xdg_mimetypes( ${DATADIR}/mime/packages )
    endif(SharedMimeInfo_FOUND)
endif()

if(BUILD_OWNCLOUD_OSX_BUNDLE)
  get_target_property (QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
  add_custom_target(macdeployqt "${CMAKE_SOURCE_DIR}/admin/osx/macdeployqt.py" "${CMAKE_INSTALL_PREFIX}/${OWNCLOUD_OSX_BUNDLE}" "${QT_QMAKE_EXECUTABLE}")
endif()
