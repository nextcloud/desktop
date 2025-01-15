set( APPLICATION_NAME       "HiDrive Next" )
set( APPLICATION_SHORTNAME  "HiDriveNext" )
set( APPLICATION_EXECUTABLE "HiDriveNext" )
set( APPLICATION_CONFIG_NAME "${APPLICATION_EXECUTABLE}" )
set( APPLICATION_DOMAIN     "ionos.com" )
set( APPLICATION_VENDOR     "IONOS SE" )
set( APPLICATION_UPDATE_URL "https://customerupdates.nextcloud.com/client/" CACHE STRING "URL for updater" )
set( APPLICATION_HELP_URL   "" CACHE STRING "URL for the help menu" )

if(APPLE AND APPLICATION_NAME STREQUAL "HiDrive Next" AND EXISTS "${CMAKE_SOURCE_DIR}/theme/colored/hidrivenext-macOS-icon.svg")
    set( APPLICATION_ICON_NAME "hidrivenext-macOS" )
    message("Using macOS-specific application icon: ${APPLICATION_ICON_NAME}")
else()
    set( APPLICATION_ICON_NAME "${APPLICATION_SHORTNAME}" )
endif()

set( APPLICATION_ICON_SET   "SVG" )
set( APPLICATION_SERVER_URL "https://easy-qa-1.nextcloud-ionos.com" CACHE STRING "URL for the server to use. If entered, the UI field will be pre-filled with it" )
set( APPLICATION_SERVER_URL_ENFORCE ON ) # If set and APPLICATION_SERVER_URL is defined, the server can only connect to the pre-defined URL
set( APPLICATION_REV_DOMAIN "com.ionos.hidrivenext.desktopclient" )
# set( APPLICATION_VIRTUALFILE_SUFFIX "nextcloud" CACHE STRING "Virtual file suffix (not including the .)")
set( APPLICATION_OCSP_STAPLING_ENABLED OFF )
set( APPLICATION_FORBID_BAD_SSL OFF )

set( LINUX_PACKAGE_SHORTNAME "hidrivenext" )
set( LINUX_APPLICATION_ID "${APPLICATION_REV_DOMAIN}.${LINUX_PACKAGE_SHORTNAME}")

set( THEME_CLASS            "NextcloudTheme" )
set( WIN_SETUP_BITMAP_PATH  "${CMAKE_SOURCE_DIR}/admin/win/nsi" )

set( MAC_INSTALLER_BACKGROUND_FILE "${CMAKE_SOURCE_DIR}/admin/osx/installer-background.png" CACHE STRING "The MacOSX installer background image")

# set( THEME_INCLUDE          "${OEM_THEME_DIR}/mytheme.h" )
# set( APPLICATION_LICENSE    "${OEM_THEME_DIR}/license.txt )

option( WITH_CRASHREPORTER "Build crashreporter" OFF )
#set( CRASHREPORTER_SUBMIT_URL "https://crash-reports.owncloud.com/submit" CACHE STRING "URL for crash reporter" )
#set( CRASHREPORTER_ICON ":/owncloud-icon.png" )

## Updater options
option( BUILD_UPDATER "Build updater" ON )

option( WITH_PROVIDERS "Build with providers list" ON )

option( ENFORCE_VIRTUAL_FILES_SYNC_FOLDER "Enforce use of virtual files sync folder when available" OFF )

option(ENFORCE_SINGLE_ACCOUNT "Enforce use of a single account in desktop client" OFF)

option( DO_NOT_USE_PROXY "Do not use system wide proxy, instead always do a direct connection to server" OFF )

## Theming options
set(NEXTCLOUD_BACKGROUND_COLOR "#0082c9" CACHE STRING "Default Nextcloud background color")
set( APPLICATION_WIZARD_HEADER_BACKGROUND_COLOR ${NEXTCLOUD_BACKGROUND_COLOR} CACHE STRING "Hex color of the wizard header background")
set( APPLICATION_WIZARD_HEADER_TITLE_COLOR "#000000" CACHE STRING "Hex color of the text in the wizard header")
option( APPLICATION_WIZARD_USE_CUSTOM_LOGO "Use the logo from ':/client/theme/colored/wizard_logo.(png|svg)' else the default application icon is used" ON )


#
## Windows Shell Extensions & MSI - IMPORTANT: Generate new GUIDs for custom builds with "guidgen" or "uuidgen"
#
if(WIN32)
    # Context Menu
    set( WIN_SHELLEXT_CONTEXT_MENU_GUID      "{28F7B423-F04D-4035-9163-742ABAB2C09D}" )

    # Overlays
    set( WIN_SHELLEXT_OVERLAY_GUID_ERROR     "{69E48F56-3877-4D15-BE6C-148D20D9AD39}" )
    set( WIN_SHELLEXT_OVERLAY_GUID_OK        "{66163328-2F7D-4727-8557-07AA1A4951D4}" )
    set( WIN_SHELLEXT_OVERLAY_GUID_OK_SHARED "{FE8E7808-F772-402A-96C5-5998ADAE34B7}" )
    set( WIN_SHELLEXT_OVERLAY_GUID_SYNC      "{BE95EBD1-B334-4D70-8797-F3827DBA7884}" )
    set( WIN_SHELLEXT_OVERLAY_GUID_WARNING   "{F5D52817-2813-4CF7-94E6-4D58D96E5EB3}" )

    # MSI Upgrade Code (without brackets)
    set( WIN_MSI_UPGRADE_CODE                "DB5332BA-8B5B-43A9-8594-48263DE4E7EA" )

    # Windows build options
    option( BUILD_WIN_MSI "Build MSI scripts and helper DLL" OFF )
    option( BUILD_WIN_TOOLS "Build Win32 migration tools" OFF )
endif()

if (APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_GREATER_EQUAL 11.0)
    option( BUILD_FILE_PROVIDER_MODULE "Build the macOS virtual files File Provider module" OFF )
endif()
