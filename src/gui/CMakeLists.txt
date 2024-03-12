project(gui)
find_package(Qt5 REQUIRED COMPONENTS Widgets Svg Qml Quick QuickControls2 Xml Network)
find_package(KF5Archive REQUIRED)

if(QUICK_COMPILER)
        find_package(Qt5QuickCompiler)
        set_package_properties(Qt5QuickCompiler PROPERTIES
                DESCRIPTION "Compile QML at build time"
                TYPE REQUIRED
        )
endif()

if (NOT TARGET Qt5::GuiPrivate)
    message(FATAL_ERROR "Could not find GuiPrivate component of Qt5. It might be shipped as a separate package, please check that.")
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DQT_QML_DEBUG)
endif()

IF(BUILD_UPDATER)
    add_subdirectory(updater)
endif()

configure_file(${CMAKE_SOURCE_DIR}/theme.qrc.in ${CMAKE_SOURCE_DIR}/theme.qrc)
set(theme_dir ${CMAKE_SOURCE_DIR}/theme)

set(client_UI_SRCS
    accountsettings.ui
    conflictdialog.ui
    invalidfilenamedialog.ui
    caseclashfilenamedialog.ui
    foldercreationdialog.ui
    folderwizardsourcepage.ui
    folderwizardtargetpage.ui
    generalsettings.ui
    legalnotice.ui
    ignorelisteditor.ui
    ignorelisttablewidget.ui
    networksettings.ui
    settingsdialog.ui
    sslerrordialog.ui
    addcertificatedialog.ui
    passwordinputdialog.ui
    proxyauthdialog.ui
    mnemonicdialog.ui
    vfsdownloaderrordialog.ui
    wizard/flow2authwidget.ui
    wizard/owncloudadvancedsetuppage.ui
    wizard/owncloudconnectionmethoddialog.ui
    wizard/owncloudhttpcredspage.ui
    wizard/owncloudsetupnocredspage.ui
    wizard/webview.ui
    wizard/welcomepage.ui
)

if(QUICK_COMPILER)
    qtquick_compiler_add_resources(client_UI_SRCS ../../resources.qrc ${CMAKE_SOURCE_DIR}/theme.qrc)
else()
    qt_add_resources(client_UI_SRCS ../../resources.qrc ${CMAKE_SOURCE_DIR}/theme.qrc)
endif()

set(client_SRCS
    accountmanager.h
    accountmanager.cpp
    accountsettings.h
    accountsettings.cpp
    accountsetupfromcommandlinejob.h
    accountsetupfromcommandlinejob.cpp
    accountsetupcommandlinemanager.h
    accountsetupcommandlinemanager.cpp
    application.h
    application.cpp
    invalidfilenamedialog.h
    invalidfilenamedialog.cpp
    caseclashfilenamedialog.h
    caseclashfilenamedialog.cpp
    callstatechecker.h
    callstatechecker.cpp
    conflictdialog.h
    conflictdialog.cpp
    conflictsolver.h
    conflictsolver.cpp
    connectionvalidator.h
    connectionvalidator.cpp
    editlocallyjob.h
    editlocallyjob.cpp
    editlocallymanager.h
    editlocallymanager.cpp
    filetagmodel.h
    filetagmodel.cpp
    folder.h
    folder.cpp
    foldercreationdialog.h
    foldercreationdialog.cpp
    folderman.h
    folderman.cpp
    folderstatusmodel.h
    folderstatusmodel.cpp
    folderstatusdelegate.h
    folderstatusdelegate.cpp
    folderstatusview.h
    folderstatusview.cpp
    folderwatcher.h
    folderwatcher.cpp
    folderwizard.h
    folderwizard.cpp
    generalsettings.h
    generalsettings.cpp
    legalnotice.h
    legalnotice.cpp
    ignorelisteditor.h
    ignorelisteditor.cpp
    ignorelisttablewidget.h
    ignorelisttablewidget.cpp
    lockwatcher.h
    lockwatcher.cpp
    logbrowser.h
    logbrowser.cpp
    navigationpanehelper.h
    navigationpanehelper.cpp
    networksettings.h
    networksettings.cpp
    ocsnavigationappsjob.h
    ocsnavigationappsjob.cpp
    ocsjob.h
    ocsjob.cpp
    ocssharejob.h
    ocssharejob.cpp
    ocsshareejob.h
    ocsshareejob.cpp
    openfilemanager.h
    openfilemanager.cpp
    owncloudgui.h
    owncloudgui.cpp
    owncloudsetupwizard.h
    owncloudsetupwizard.cpp
    passwordinputdialog.h
    passwordinputdialog.cpp
    selectivesyncdialog.h
    selectivesyncdialog.cpp
    settingsdialog.h
    settingsdialog.cpp
    sharemanager.h
    sharemanager.cpp
    profilepagewidget.h
    profilepagewidget.cpp
    sharee.h
    sharee.cpp
    sslbutton.h
    sslbutton.cpp
    sslerrordialog.h
    sslerrordialog.cpp
    syncrunfilelog.h
    syncrunfilelog.cpp
    systray.h
    systray.cpp
    thumbnailjob.h
    thumbnailjob.cpp
    userinfo.h
    userinfo.cpp
    vfsdownloaderrordialog.h
    vfsdownloaderrordialog.cpp
    accountstate.h
    accountstate.cpp
    addcertificatedialog.h
    addcertificatedialog.cpp
    authenticationdialog.h
    authenticationdialog.cpp
    proxyauthhandler.h
    proxyauthhandler.cpp
    proxyauthdialog.h
    proxyauthdialog.cpp
    tooltipupdater.h
    tooltipupdater.cpp
    notificationconfirmjob.h
    notificationconfirmjob.cpp
    guiutility.h
    guiutility.cpp
    elidedlabel.h
    elidedlabel.cpp
    iconutils.h
    iconutils.cpp
    remotewipe.h
    remotewipe.cpp
    userstatusselectormodel.h
    userstatusselectormodel.cpp
    emojimodel.h
    emojimodel.cpp
    syncconflictsmodel.h
    syncconflictsmodel.cpp
    fileactivitylistmodel.h
    fileactivitylistmodel.cpp
    filedetails/datefieldbackend.h
    filedetails/datefieldbackend.cpp
    filedetails/filedetails.h
    filedetails/filedetails.cpp
    filedetails/sharemodel.h
    filedetails/sharemodel.cpp
    filedetails/shareemodel.h
    filedetails/shareemodel.cpp
    filedetails/sortedsharemodel.h
    filedetails/sortedsharemodel.cpp
    tray/svgimageprovider.h
    tray/svgimageprovider.cpp
    tray/syncstatussummary.h
    tray/syncstatussummary.cpp
    tray/activitydata.h
    tray/activitydata.cpp
    tray/activitylistmodel.h
    tray/activitylistmodel.cpp
    tray/unifiedsearchresult.h
    tray/asyncimageresponse.cpp
    tray/unifiedsearchresult.cpp
    tray/unifiedsearchresultslistmodel.h
    tray/trayimageprovider.cpp
    tray/unifiedsearchresultslistmodel.cpp
    tray/usermodel.h
    tray/usermodel.cpp
    tray/notificationhandler.h
    tray/notificationhandler.cpp
    tray/sortedactivitylistmodel.h
    tray/sortedactivitylistmodel.cpp
    creds/credentialsfactory.h
    tray/talkreply.cpp
    creds/credentialsfactory.cpp
    creds/httpcredentialsgui.h
    creds/httpcredentialsgui.cpp
    creds/flow2auth.h
    creds/flow2auth.cpp
    creds/webflowcredentials.h
    creds/webflowcredentials.cpp
    creds/webflowcredentialsdialog.h
    creds/webflowcredentialsdialog.cpp
    wizard/postfixlineedit.h
    wizard/postfixlineedit.cpp
    wizard/abstractcredswizardpage.h
    wizard/abstractcredswizardpage.cpp
    wizard/owncloudadvancedsetuppage.h
    wizard/owncloudadvancedsetuppage.cpp
    wizard/owncloudconnectionmethoddialog.h
    wizard/owncloudconnectionmethoddialog.cpp
    wizard/owncloudhttpcredspage.h
    wizard/owncloudhttpcredspage.cpp
    wizard/flow2authcredspage.h
    wizard/flow2authcredspage.cpp
    wizard/flow2authwidget.h
    wizard/flow2authwidget.cpp
    wizard/owncloudsetuppage.h
    wizard/owncloudsetuppage.cpp
    wizard/owncloudwizardcommon.h
    wizard/owncloudwizardcommon.cpp
    wizard/owncloudwizard.h
    wizard/owncloudwizard.cpp
    wizard/slideshow.h
    wizard/slideshow.cpp
    wizard/welcomepage.h
    wizard/welcomepage.cpp
    wizard/linklabel.h
    wizard/linklabel.cpp
    )

if (Qt5WebEngine_FOUND AND Qt5WebEngineWidgets_FOUND)
  list(APPEND client_SRCS
    wizard/webviewpage.h
    wizard/webviewpage.cpp
    wizard/webview.h
    wizard/webview.cpp
  )
endif()

IF(BUILD_UPDATER)
    set(updater_SRCS
        updater/ocupdater.h
        updater/ocupdater.cpp
        updater/updateinfo.h
        updater/updateinfo.cpp
        updater/updater.h
        updater/updater.cpp
    )
endif()

IF( APPLE )
    list(APPEND client_SRCS cocoainitializer_mac.mm)
    list(APPEND client_SRCS systray_mac_common.mm)

    if (CMAKE_OSX_DEPLOYMENT_TARGET VERSION_GREATER_EQUAL 10.14)
        list(APPEND client_SRCS systray_mac_usernotifications.mm)
    endif()

    if (BUILD_FILE_PROVIDER_MODULE)
        list(APPEND client_SRCS
            macOS/fileprovider.h
            macOS/fileprovider_mac.mm
            macOS/fileproviderdomainmanager.h
            macOS/fileproviderdomainmanager_mac.mm
            macOS/fileprovidersocketcontroller.h
            macOS/fileprovidersocketcontroller.cpp
            macOS/fileprovidersocketserver.h
            macOS/fileprovidersocketserver.cpp
            macOS/fileprovidersocketserver_mac.mm)
    endif()

    if(SPARKLE_FOUND AND BUILD_UPDATER)
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
set(client_SRCS ${client_SRCS} folderwatcher_win.cpp shellextensionsserver.cpp ${CMAKE_SOURCE_DIR}/src/common/shellextensionutils.cpp)
ENDIF()
IF( APPLE )
list(APPEND client_SRCS folderwatcher_mac.cpp)
ENDIF()

set(3rdparty_SRC
    ../3rdparty/QProgressIndicator/QProgressIndicator.h
    ../3rdparty/QProgressIndicator/QProgressIndicator.cpp
    ../3rdparty/qtlockedfile/qtlockedfile.h
    ../3rdparty/qtlockedfile/qtlockedfile.cpp
    ../3rdparty/qtsingleapplication/qtlocalpeer.h
    ../3rdparty/qtsingleapplication/qtlocalpeer.cpp
    ../3rdparty/qtsingleapplication/qtsingleapplication.h
    ../3rdparty/qtsingleapplication/qtsingleapplication.cpp
    ../3rdparty/qtsingleapplication/qtsinglecoreapplication.h
    ../3rdparty/qtsingleapplication/qtsinglecoreapplication.cpp
    ../3rdparty/kmessagewidget/kmessagewidget.h
    ../3rdparty/kmessagewidget/kmessagewidget.cpp
    ../3rdparty/kirigami/wheelhandler.h
    ../3rdparty/kirigami/wheelhandler.cpp
   )

set_property(SOURCE ../3rdparty/kmessagewidget/kmessagewidget.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
set_property(SOURCE ../3rdparty/kirigami/wheelhandler.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)

if(NOT WIN32)
   list(APPEND 3rdparty_SRC ../3rdparty/qtlockedfile/qtlockedfile_unix.cpp)
   set_property(SOURCE ../3rdparty/qtlockedfile/qtlockedfile_unix.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
else()
   list(APPEND 3rdparty_SRC ../3rdparty/qtlockedfile/qtlockedfile_win.cpp )
   set_property(SOURCE ../3rdparty/qtlockedfile/qtlockedfile_win.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
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
    ${client_SRCS}
    ${client_UI_SRCS}
    ${guiMoc}
    ${client_I18N}
    ${3rdparty_SRC}
    ${3rdparty_MOC}
)

if(Qt5Keychain_FOUND)
    list(APPEND libsync_LINK_TARGETS qt5keychain)
endif()

# add executable icon on windows and osx
include(GenerateIconsUtils)

# For historical reasons we can not use the application_shortname
# for ownCloud but must rather set it manually.
if (NOT DEFINED APPLICATION_ICON_NAME)
    set(APPLICATION_ICON_NAME ${APPLICATION_SHORTNAME})
endif()

if(NOT DEFINED APPLICATION_FOLDER_ICON_INDEX)
    set(APPLICATION_FOLDER_ICON_INDEX 0)
endif()

set(STATE_ICONS_COLORS colored black white)

foreach(state_icons_color ${STATE_ICONS_COLORS})
    set(STATE_ICONS_PATH "${theme_dir}/${state_icons_color}/")

    message("Generating state icons from SVG in path: ${STATE_ICONS_PATH}")

    file(GLOB_RECURSE STATE_ICONS_SVG "${STATE_ICONS_PATH}/state-*.svg")

    foreach(state_icon_svg ${STATE_ICONS_SVG})
        get_filename_component(status_icon_name_wle ${state_icon_svg} NAME_WLE)
        foreach(size IN ITEMS 16;32;64;128;256)
            generate_sized_png_from_svg(${state_icon_svg} ${size} OUTPUT_ICON_FULL_NAME_WLE "${status_icon_name_wle}-${size}")
        endforeach()
    endforeach()
endforeach()

if ((APPLICATION_ICON_SET MATCHES "PNG")
    AND
    (NOT EXISTS "${theme_dir}/colored/${APPLICATION_ICON_NAME}-icon.svg"))
  # We may have no svg application icon in when customers use PNG
  # icons in brander, but theme.qrc expects a svg icon.
  file(TOUCH "${theme_dir}/colored/${APPLICATION_ICON_NAME}-icon.svg")
endif()

if(APPLE)
  set(MACOS_SIDEBAR_ICON_SVG "${theme_dir}/colored/${APPLICATION_ICON_NAME}-sidebar.svg")
  generate_sized_png_from_svg(${MACOS_SIDEBAR_ICON_SVG} 16)
  generate_sized_png_from_svg(${MACOS_SIDEBAR_ICON_SVG} 32)
  generate_sized_png_from_svg(${MACOS_SIDEBAR_ICON_SVG} 64)
  generate_sized_png_from_svg(${MACOS_SIDEBAR_ICON_SVG} 128)
  generate_sized_png_from_svg(${MACOS_SIDEBAR_ICON_SVG} 256)
endif()

if(WIN32)
  set(STARTMENU_ICON_SVG "${theme_dir}/colored/${APPLICATION_ICON_NAME}-w10startmenu.svg")
  generate_sized_png_from_svg(${STARTMENU_ICON_SVG} 70)
  generate_sized_png_from_svg(${STARTMENU_ICON_SVG} 150)
endif()

set(APP_ICON_SVG "${theme_dir}/colored/${APPLICATION_ICON_NAME}-icon.svg")

# generate secondary icon if available (currently for Windows only)--------------------------------------
set(APP_SECONDARY_ICONS "${theme_dir}/colored/icons")
set(APP_ICON_WIN_FOLDER_SVG "${APP_SECONDARY_ICONS}/${APPLICATION_ICON_NAME}-icon-win-folder.svg")

set(RC_DEPENDENCIES "")

if(WIN32)
    if (EXISTS ${APP_ICON_WIN_FOLDER_SVG})
        get_filename_component(output_icon_name_win ${APP_ICON_WIN_FOLDER_SVG} NAME_WLE)
        # Product icon (for smallest size)
        foreach(size IN ITEMS 16;20)
            generate_sized_png_from_svg(${APP_ICON_SVG} ${size} OUTPUT_ICON_NAME ${output_icon_name_win} OUTPUT_ICON_PATH "${APP_SECONDARY_ICONS}/")
        endforeach()

        # Product icon with Windows folder (for sizes larger than 20)
        foreach(size IN ITEMS 24;32;40;48;64;128;256;512;1024)
            generate_sized_png_from_svg(${APP_ICON_WIN_FOLDER_SVG} ${size} OUTPUT_ICON_NAME ${output_icon_name_win} OUTPUT_ICON_PATH "${APP_SECONDARY_ICONS}/")
        endforeach()

        file(GLOB_RECURSE OWNCLOUD_ICONS_WIN_FOLDER "${APP_SECONDARY_ICONS}/*-${APPLICATION_ICON_NAME}-icon*")
        set(APP_ICON_WIN_FOLDER_ICO_NAME "${APPLICATION_ICON_NAME}-win-folder")
        set(RC_DEPENDENCIES "${RC_DEPENDENCIES} ${APP_ICON_WIN_FOLDER_ICO_NAME}.ico")
        ecm_add_app_icon(APP_ICON_WIN_FOLDER ICONS "${OWNCLOUD_ICONS_WIN_FOLDER}" SIDEBAR_ICONS "${OWNCLOUD_SIDEBAR_ICONS}" OUTFILE_BASENAME "${APP_ICON_WIN_FOLDER_ICO_NAME}" ICON_INDEX 2)
    endif()
endif()
# --------------------------------------

if (NOT ${RC_DEPENDENCIES} STREQUAL "")
    string(STRIP ${RC_DEPENDENCIES} RC_DEPENDENCIES)
endif()

# generate primary icon from SVG (due to Win .ico vs .rc dependency issues, primary icon must always be generated last)--------------------------------------
if(WIN32)
    foreach(size IN ITEMS 16;20;24;32;40;48;64;128;256;512;1024)
        generate_sized_png_from_svg(${APP_ICON_SVG} ${size})
    endforeach()
else()
    foreach(size IN ITEMS 16;24;32;48;64;128;256;512;1024)
        generate_sized_png_from_svg(${APP_ICON_SVG} ${size})
    endforeach()
endif()

file(GLOB_RECURSE OWNCLOUD_ICONS "${theme_dir}/colored/*-${APPLICATION_ICON_NAME}-icon*")

if(APPLE)
    file(GLOB_RECURSE OWNCLOUD_SIDEBAR_ICONS "${theme_dir}/colored/*-${APPLICATION_ICON_NAME}-sidebar*")
    MESSAGE(STATUS "OWNCLOUD_SIDEBAR_ICONS: ${APPLICATION_ICON_NAME}: ${OWNCLOUD_SIDEBAR_ICONS}")
endif()

ecm_add_app_icon(APP_ICON RC_DEPENDENCIES ${RC_DEPENDENCIES} ICONS "${OWNCLOUD_ICONS}" SIDEBAR_ICONS "${OWNCLOUD_SIDEBAR_ICONS}" OUTFILE_BASENAME "${APPLICATION_ICON_NAME}" ICON_INDEX 1)
# --------------------------------------

if(WIN32)
# merge *.rc.in files for Windows (multiple ICON resources must be placed in a single file, otherwise, this won't work de to a bug in Windows compiler https://developercommunity.visualstudio.com/t/visual-studio-2017-prof-1557-cvt1100-duplicate-res/363156)
    function(merge_files IN_FILE OUT_FILE)
        file(READ ${IN_FILE} CONTENTS)
        message("Merging ${IN_FILE} into ${OUT_FILE}")
        file(APPEND ${OUT_FILE} "${CONTENTS}")
    endfunction()
    message("APP_ICON is: ${APP_ICON}")
    if(APP_ICON)
        get_filename_component(RC_IN_FOLDER ${APP_ICON}} DIRECTORY)

        file(GLOB_RECURSE RC_IN_FILES "${RC_IN_FOLDER}/*rc.in")

        foreach(rc_in_file IN ITEMS ${RC_IN_FILES})
          get_filename_component(rc_in_file_name ${rc_in_file} NAME)
          get_filename_component(app_icon_name "${APP_ICON}.in" NAME)
          if(NOT "${rc_in_file_name}" STREQUAL "${app_icon_name}")
              merge_files(${rc_in_file} "${APP_ICON}.in")
              if (DEFINED APPLICATION_FOLDER_ICON_INDEX)
                  MATH(EXPR APPLICATION_FOLDER_ICON_INDEX "${APPLICATION_FOLDER_ICON_INDEX}+1")
                  message("APPLICATION_FOLDER_ICON_INDEX is now set to: ${APPLICATION_FOLDER_ICON_INDEX}")
              endif()
          endif()
        endforeach()
    endif()
endif()
# --------------------------------------

if(UNIX AND NOT APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
endif()

add_library(nextcloudCore STATIC ${final_src})

target_link_libraries(nextcloudCore
  PUBLIC
  Nextcloud::sync
  Qt5::Widgets
  Qt5::GuiPrivate
  Qt5::Svg
  Qt5::Network
  Qt5::Xml
  Qt5::Qml
  Qt5::Quick
  Qt5::QuickControls2
  KF5::Archive
  )

add_subdirectory(socketapi)

# skip unity inclusion for files which cause problems with a CMake unity build
set_property(SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/socketapi/socketapi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/socketapi/socketuploadjob.cpp
    PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)

foreach(FILE IN LISTS client_UI_SRCS)
	set_property(SOURCE ${FILE} PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endforeach()

if(Qt5WebEngine_FOUND AND Qt5WebEngineWidgets_FOUND)
  target_link_libraries(nextcloudCore PUBLIC Qt5::WebEngineWidgets)
endif()

set_target_properties(nextcloudCore
  PROPERTIES
  AUTOUIC ON
  AUTOMOC ON
)

target_include_directories(nextcloudCore
  PUBLIC
  ${CMAKE_SOURCE_DIR}/src/3rdparty/QProgressIndicator
  ${CMAKE_SOURCE_DIR}/src/3rdparty/qtlockedfile
  ${CMAKE_SOURCE_DIR}/src/3rdparty/kirigami
  ${CMAKE_SOURCE_DIR}/src/3rdparty/qtsingleapplication
  ${CMAKE_SOURCE_DIR}/src/3rdparty/kmessagewidget
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE)
    if(NOT WIN32)
        file(GLOB _icons "${theme_dir}/colored/*-${APPLICATION_ICON_NAME}-icon.png")
        foreach(_file ${_icons})
            string(REPLACE "${theme_dir}/colored/" "" _res ${_file})
            string(REPLACE "-${APPLICATION_ICON_NAME}-icon.png" "" _res ${_res})
            install(FILES ${_file} RENAME ${APPLICATION_ICON_NAME}.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_res}x${_res}/apps)
        endforeach(_file)
        install(FILES ${client_I18N} DESTINATION ${CMAKE_INSTALL_DATADIR}/${APPLICATION_EXECUTABLE}/i18n)
    else()
        file(GLOB_RECURSE VISUAL_ELEMENTS "${theme_dir}/colored/*-${APPLICATION_ICON_NAME}-w10startmenu*")
        install(FILES ${VISUAL_ELEMENTS} DESTINATION bin/visualelements)
        install(FILES "${theme_dir}/${APPLICATION_EXECUTABLE}.VisualElementsManifest.xml" DESTINATION bin)
        install(FILES ${client_I18N} DESTINATION i18n)
    endif()

    # we may not add MACOSX_BUNDLE here, if not building one
    add_executable(nextcloud WIN32 main.cpp ${client_version} ${client_manifest} ${APP_ICON})
    set_target_properties(nextcloud PROPERTIES
      OUTPUT_NAME "${APPLICATION_EXECUTABLE}"
    )
else()
    # set(CMAKE_INSTALL_PREFIX ".") # Examples use /Applications. hurmpf.
    set(MACOSX_BUNDLE_ICON_FILE "${APPLICATION_ICON_NAME}.icns")

    # we must add MACOSX_BUNDLE only if building a bundle
    add_executable(nextcloud WIN32 MACOSX_BUNDLE main.cpp ${APP_ICON})

    if (BUILD_OWNCLOUD_OSX_BUNDLE)
      set_target_properties(nextcloud PROPERTIES
        OUTPUT_NAME "${APPLICATION_NAME}")
    else()
      set_target_properties(nextcloud PROPERTIES
        OUTPUT_NAME "${APPLICATION_EXECUTABLE}")
    endif()

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

IF(BUILD_UPDATER)
    add_library(updater STATIC ${updater_SRCS})
    target_link_libraries(updater Nextcloud::sync ${updater_DEPS} Qt5::Widgets Qt5::Svg Qt5::Network Qt5::Xml)
    target_include_directories(updater PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(updater PROPERTIES AUTOMOC ON)
    target_link_libraries(nextcloudCore PUBLIC updater)
endif()

set_target_properties(nextcloud PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

target_link_libraries(nextcloud PRIVATE nextcloudCore)

if(TARGET PkgConfig::CLOUDPROVIDERS)
    message("Building with libcloudproviderssupport")
    target_sources(nextcloudCore PRIVATE cloudproviders/cloudprovidermanager.cpp cloudproviders/cloudproviderwrapper.cpp)

    string(TOLOWER "${APPLICATION_VENDOR}" DBUS_VENDOR)
    string(REGEX REPLACE "[^A-z0-9]" "" DBUS_VENDOR "${DBUS_VENDOR}")
    string(REGEX REPLACE "[^A-z0-9]" "" DBUS_APPLICATION_NAME "${APPLICATION_SHORTNAME}")
    if(NOT DBUS_PREFIX)
        set(DBUS_PREFIX "com")
    endif(NOT DBUS_PREFIX)
    set(LIBCLOUDPROVIDERS_DBUS_BUS_NAME "${DBUS_PREFIX}.${DBUS_VENDOR}.${DBUS_APPLICATION_NAME}")
    set(LIBCLOUDPROVIDERS_DBUS_OBJECT_PATH "/${DBUS_PREFIX}/${DBUS_VENDOR}/${DBUS_APPLICATION_NAME}")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cloudproviders/cloudproviderconfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/cloudproviderconfig.h)
    target_compile_definitions(nextcloudCore PUBLIC -DWITH_LIBCLOUDPROVIDERS)
    target_link_libraries(nextcloudCore
      PRIVATE
      PkgConfig::CLOUDPROVIDERS
      PkgConfig::GLIB2
      PkgConfig::GIO
    )

    list(APPEND LIBCLOUDPROVIDERS_DESKTOP_IMPLEMENTS "Implements=org.freedesktop.CloudProviders\n")
    list(APPEND LIBCLOUDPROVIDERS_DESKTOP_IMPLEMENTS "[org.freedesktop.CloudProviders]")
    list(APPEND LIBCLOUDPROVIDERS_DESKTOP_IMPLEMENTS "BusName=${LIBCLOUDPROVIDERS_DBUS_BUS_NAME}")
    list(APPEND LIBCLOUDPROVIDERS_DESKTOP_IMPLEMENTS "ObjectPath=${LIBCLOUDPROVIDERS_DBUS_OBJECT_PATH}")
    list(JOIN LIBCLOUDPROVIDERS_DESKTOP_IMPLEMENTS "\n" LIBCLOUDPROVIDERS_DESKTOP_IMPLEMENTS)
endif()

## handle DBUS for Fdo notifications
if( UNIX AND NOT APPLE )
    find_package(Qt5 COMPONENTS DBus)
    target_link_libraries(nextcloudCore PUBLIC Qt5::DBus)
    target_compile_definitions(nextcloudCore PUBLIC "USE_FDO_NOTIFICATIONS")
endif()

if (APPLE)
    find_package(Qt5 COMPONENTS MacExtras)

    if (BUILD_FILE_PROVIDER_MODULE)
        target_link_libraries(nextcloudCore PUBLIC Qt5::MacExtras "-framework UserNotifications -framework FileProvider")
    elseif(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_GREATER_EQUAL 10.14)
        target_link_libraries(nextcloudCore PUBLIC Qt5::MacExtras "-framework UserNotifications")
    else()
        target_link_libraries(nextcloudCore PUBLIC Qt5::MacExtras)
    endif()
endif()

if(WITH_CRASHREPORTER)
    target_link_libraries(nextcloudCore PUBLIC crashreporter-handler)

    if(UNIX AND NOT MAC)
        find_package(Threads REQUIRED)
        target_link_libraries(nextcloudCore PUBLIC Threads::Threads)
    endif()
endif()

install(TARGETS nextcloud
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        BUNDLE  DESTINATION "."
 )


# FIXME: The following lines are dup in src/gui and src/cmd because it needs to be done after both are installed
#FIXME: find a nice solution to make the second if(BUILD_OWNCLOUD_OSX_BUNDLE) unnecessary
# currently it needs to be done because the code right above needs to be executed no matter
# if building a bundle or not and the install_qt4_executable needs to be called afterwards
#
# OSX: Run macdeployqt for src/gui and for src/cmd using the -executable option
if(BUILD_OWNCLOUD_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY)
    get_target_property (QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${QT_BIN_DIR}")

    set(cmd_NAME ${APPLICATION_EXECUTABLE}cmd)

    if(CMAKE_BUILD_TYPE MATCHES Debug)
        set(NO_STRIP "-no-strip")
    else()
        set(NO_STRIP "")
    endif()

    add_custom_command(TARGET nextcloud POST_BUILD
        COMMAND "${MACDEPLOYQT_EXECUTABLE}"
        "$<TARGET_FILE_DIR:nextcloud>/../.."
        -qmldir=${CMAKE_SOURCE_DIR}/src/gui
        -always-overwrite
        -executable="$<TARGET_FILE_DIR:nextcloud>/${cmd_NAME}"
        ${NO_STRIP}
        COMMAND "${CMAKE_COMMAND}"
        -E rm -rf "${BIN_OUTPUT_DIRECTORY}/${OWNCLOUD_OSX_BUNDLE}/Contents/PlugIns/bearer"
        COMMENT "Running macdeployqt..."
    )
endif()

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE AND NOT WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/mirall.desktop.in
                   ${CMAKE_CURRENT_BINARY_DIR}/${LINUX_APPLICATION_ID}.desktop)
    install(FILES  ${CMAKE_CURRENT_BINARY_DIR}/${LINUX_APPLICATION_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications )

    configure_file(owncloud.xml.in ${APPLICATION_EXECUTABLE}.xml)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_EXECUTABLE}.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages )

    find_package(SharedMimeInfo)
    if(SharedMimeInfo_FOUND)
        update_xdg_mimetypes( ${CMAKE_INSTALL_DATADIR}/mime/packages )
    endif(SharedMimeInfo_FOUND)
endif()

configure_file(configgui.h.in ${CMAKE_CURRENT_BINARY_DIR}/configgui.h)
