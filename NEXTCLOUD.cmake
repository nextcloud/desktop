set( APPLICATION_NAME       "Nextcloud" )
set( APPLICATION_SHORTNAME  "Nextcloud" )
set( APPLICATION_EXECUTABLE "nextcloud" )
set( APPLICATION_DOMAIN     "nextcloud.com" )
set( APPLICATION_VENDOR     "Nextcloud GmbH" )
set( APPLICATION_UPDATE_URL "https://updates.nextcloud.org/client/" CACHE string "URL for updater" )
set( APPLICATION_ICON_NAME  "Nextcloud" )

set( LINUX_PACKAGE_SHORTNAME "nextcloud" )

set( THEME_CLASS            "NextcloudTheme" )
set( APPLICATION_REV_DOMAIN "com.nextcloud.desktopclient" )
set( WIN_SETUP_BITMAP_PATH  "${CMAKE_SOURCE_DIR}/admin/win/nsi" )

set( MAC_INSTALLER_BACKGROUND_FILE "${CMAKE_SOURCE_DIR}/admin/osx/installer-background.png" CACHE STRING "The MacOSX installer background image")

# set( THEME_INCLUDE          "${OEM_THEME_DIR}/mytheme.h" )
# set( APPLICATION_LICENSE    "${OEM_THEME_DIR}/license.txt )

option( WITH_CRASHREPORTER "Build crashreporter" OFF )
#set( CRASHREPORTER_SUBMIT_URL "https://crash-reports.owncloud.com/submit" CACHE string "URL for crash reporter" )
#set( CRASHREPORTER_ICON ":/owncloud-icon.png" )
