include(ECMAddAppIcon)

find_package(KDSingleApplication-qt6 1.0.0 REQUIRED)

set(client_UI_SRCS
    aboutdialog.ui
    accountsettings.ui
    generalsettings.ui
    ignorelisteditor.ui
    networksettings.ui
    protocolwidget.ui
    issueswidget.ui
    activitywidget.ui
    settingsdialog.ui
    sharedialog.ui
    sharelinkwidget.ui
    shareusergroupwidget.ui
    shareuserline.ui
    tlserrordialog.ui
    notificationwidget.ui
    logbrowser.ui
)

set(client_SRCS
    aboutdialog.cpp
    accountmanager.cpp
    accountsettings.cpp

    accountmodalwidget.cpp
    accountmodalwidget.ui

    application.cpp
    fetchserversettings.cpp
    clientproxy.cpp
    commonstrings.cpp
    connectionvalidator.cpp
    folder.cpp
    folderman.cpp
    folderstatusmodel.cpp
    folderwatcher.cpp
    generalsettings.cpp
    ignorelisteditor.cpp
    lockwatcher.cpp
    logbrowser.cpp
    networksettings.cpp
    ocssharejob.cpp
    openfilemanager.cpp
    owncloudgui.cpp
    protocolwidget.cpp
    protocolitem.cpp
    issueswidget.cpp
    activitydata.cpp
    activitywidget.cpp
    selectivesyncwidget.cpp
    settingsdialog.cpp
    sharedialog.cpp
    sharelinkwidget.cpp
    sharemanager.cpp
    shareusergroupwidget.cpp
    sharee.cpp
    tlserrordialog.cpp
    syncrunfilelog.cpp
    systray.cpp
    thumbnailjob.cpp
    quotainfo.cpp
    accountstate.cpp
    notificationwidget.cpp
    notificationconfirmjob.cpp
    servernotificationhandler.cpp
    guiutility.cpp
    elidedlabel.cpp
    translations.cpp
    creds/httpcredentialsgui.cpp
    updateurldialog.cpp

    models/activitylistmodel.cpp
    models/expandingheaderview.cpp
    models/models.cpp
    models/protocolitemmodel.cpp

    spacemigration.cpp

    scheduling/syncscheduler.cpp
    scheduling/etagwatcher.cpp
)

set(3rdparty_SRC
    ../3rdparty/QProgressIndicator/QProgressIndicator.cpp
   )

set( final_src
    ${MIRALL_RC_SRC}
    ${client_SRCS}
    ${client_UI_SRCS}
    ${3rdparty_SRC}
)

add_subdirectory(newwizard)
add_subdirectory(folderwizard)
add_subdirectory(loginrequireddialog)

add_library(owncloudCore STATIC ${final_src})
set_target_properties(owncloudCore PROPERTIES AUTOUIC ON AUTORCC ON)
# for the generated qml module
target_include_directories(owncloudCore PRIVATE models)
target_link_libraries(owncloudCore
    PUBLIC
        Qt::Widgets Qt::Network Qt::Xml Qt::Quick Qt::QuickWidgets Qt::QuickControls2
        newwizard folderwizard spaces loginrequireddialog
        libsync
        Qt6Keychain::Qt6Keychain
)

apply_common_target_settings(owncloudCore)
qt_add_qml_module(owncloudCore
        URI org.ownCloud.qmlcomponents
        VERSION 1.0
        NAMESPACE OCC
        # compat to qt < 6.5
        RESOURCE_PREFIX "/qt/qml/"
        QML_FILES
            qml/SpaceDelegate.qml
            qml/FolderDelegate.qml
            qml/FolderError.qml
)

target_link_libraries(owncloudCore PUBLIC owncloudCoreplugin)

add_subdirectory(spaces)


add_subdirectory(socketapi)

target_include_directories(owncloudCore PUBLIC
    ${CMAKE_SOURCE_DIR}/src/3rdparty/QProgressIndicator
    ${CMAKE_CURRENT_BINARY_DIR}
)

IF( APPLE )
    target_sources(owncloudCore PRIVATE
            systray.mm
            settingsdialog_mac.mm
            guiutility_mac.mm
            folderwatcher_mac.cpp)
    set_source_files_properties(guiutility_mac.mm PROPERTIES COMPILE_DEFINITIONS SOCKETAPI_TEAM_IDENTIFIER_PREFIX="${SOCKETAPI_TEAM_IDENTIFIER_PREFIX}")
elseif( WIN32 )
    target_sources(owncloudCore PRIVATE
            guiutility_win.cpp
            folderwatcher_win.cpp)
elseif(UNIX AND NOT APPLE)
    ## handle DBUS for Fdo notifications
    if (TARGET Qt::DBus)
        target_link_libraries(owncloudCore PUBLIC Qt::DBus)
        target_compile_definitions(owncloudCore PUBLIC "USE_FDO_NOTIFICATIONS")
    endif()
    target_sources(owncloudCore PRIVATE
            folderwatcher_linux.cpp
            guiutility_unix.cpp)
endif()

if(WITH_AUTO_UPDATER)
    add_subdirectory(updater)
    target_compile_definitions(owncloudCore PRIVATE WITH_AUTO_UPDATER)
endif()

add_executable(owncloud main.cpp)
set_target_properties(owncloud PROPERTIES
    OUTPUT_NAME "${APPLICATION_EXECUTABLE}"
    AUTOUIC ON
    AUTORCC ON
)
apply_common_target_settings(owncloud)
target_link_libraries(owncloud PUBLIC owncloudCore owncloudResources KDAB::kdsingleapplication )

MESSAGE(STATUS "OWNCLOUD_SIDEBAR_ICONS: ${APPLICATION_ICON_NAME}: ${OWNCLOUD_SIDEBAR_ICONS}")

ecm_add_app_icon(appIcons ICONS "${OWNCLOUD_ICONS}" SIDEBAR_ICONS "${OWNCLOUD_SIDEBAR_ICONS}" OUTFILE_BASENAME "${APPLICATION_ICON_NAME}")
target_sources(owncloud PRIVATE ${appIcons})

if(NOT APPLE)
    if(WIN32)
        target_sources(owncloud PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/owncloud.exe.manifest)
    else()
        file(GLOB _icons "${OEM_THEME_DIR}/theme/colored/*-${APPLICATION_ICON_NAME}-icon.png")
        foreach(_file ${_icons})
            string(REPLACE "${OEM_THEME_DIR}/theme/colored/" "" _res ${_file})
            string(REPLACE "-${APPLICATION_ICON_NAME}-icon.png" "" _res ${_res})
            install(FILES ${_file} RENAME ${APPLICATION_ICON_NAME}.png DESTINATION ${KDE_INSTALL_DATADIR}/icons/hicolor/${_res}x${_res}/apps)
        endforeach(_file)
    endif()

    # we may not add MACOSX_BUNDLE here, if not building one
else()
    target_sources(owncloud PRIVATE ${OWNCLOUD_BUNDLED_RESOURCES})

    set_source_files_properties(
      ${OWNCLOUD_BUNDLED_RESOURCES}
      PROPERTIES
      MACOSX_PACKAGE_LOCATION Resources
      )

  set_target_properties(owncloud PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist)
endif()

install(TARGETS owncloud ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

if(UNIX AND NOT APPLE)
    configure_file(${CMAKE_SOURCE_DIR}/owncloud.desktop.in
                   ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_EXECUTABLE}.desktop)
    install(FILES  ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_EXECUTABLE}.desktop DESTINATION ${KDE_INSTALL_DATADIR}/applications )

    configure_file(owncloud.xml.in ${APPLICATION_EXECUTABLE}.xml)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_EXECUTABLE}.xml DESTINATION ${KDE_INSTALL_DATADIR}/mime/packages )

    find_package(SharedMimeInfo)
    if(SharedMimeInfo_FOUND)
        update_xdg_mimetypes( ${KDE_INSTALL_DATADIR}/mime/packages )
    endif(SharedMimeInfo_FOUND)
endif()
