
cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.8.0)
if(POLICY CMP0020)
  cmake_policy(SET CMP0020 NEW)
endif()

project(client)

set(BIN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(OEM_THEME_DIR "" CACHE STRING "Define directory containing a custom theme")
if ( EXISTS ${OEM_THEME_DIR}/OEM.cmake )
     include ( ${OEM_THEME_DIR}/OEM.cmake )
else ()
     include ( ${CMAKE_SOURCE_DIR}/OWNCLOUD.cmake )
endif()
# need this logic to not mess with re/uninstallations via macosx.pkgproj
if(${APPLICATION_REV_DOMAIN} STREQUAL "com.owncloud.desktopclient")
    set(APPLICATION_REV_DOMAIN_INSTALLER "com.ownCloud.client")
else()
    set(APPLICATION_REV_DOMAIN_INSTALLER ${APPLICATION_REV_DOMAIN})
endif()

# For usage in XML files we preprocess
string(REPLACE "&" "&amp;" APPLICATION_NAME_XML_ESCAPED "${APPLICATION_NAME}")
string(REPLACE "<" "&lt;" APPLICATION_NAME_XML_ESCAPED "${APPLICATION_NAME_XML_ESCAPED}")
string(REPLACE ">" "&gt;" APPLICATION_NAME_XML_ESCAPED "${APPLICATION_NAME_XML_ESCAPED}")

if (NOT DEFINED LINUX_PACKAGE_SHORTNAME)
    set(LINUX_PACKAGE_SHORTNAME "${APPLICATION_SHORTNAME}")
endif()

if (NOT DEFINED PACKAGE)
    set(PACKAGE "${LINUX_PACKAGE_SHORTNAME}-client")
endif()

set( CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules )

if(NOT CRASHREPORTER_EXECUTABLE)
    set(CRASHREPORTER_EXECUTABLE "${APPLICATION_EXECUTABLE}_crash_reporter")
endif()

include(Warnings)

include(${CMAKE_SOURCE_DIR}/VERSION.cmake)
# For config.h
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
# Allows includes based on src/ like #include "common/utility.h" or #include "csync/csync.h"
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/src
)

# disable the crashreporter if libcrashreporter-qt is not available or we're building for ARM
if( CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/libcrashreporter-qt/CMakeLists.txt")
    set( WITH_CRASHREPORTER OFF )
endif()

if(NOT WITH_CRASHREPORTER)
    message(STATUS "Build of crashreporter disabled.")
endif()

#####
## handle DBUS for Fdo notifications
if( UNIX AND NOT APPLE )
    add_definitions( -DUSE_FDO_NOTIFICATIONS)
    set(WITH_DBUS ON)
endif()
####

include(GNUInstallDirs)
include(DefineInstallationPaths)
include(GenerateExportHeader)

include(GetGitRevisionDescription)

get_git_head_revision(GIT_REFSPEC GIT_SHA1)

# if we cannot get it from git, directly try .tag (packages)
# this will work if the tar balls have been properly created
# via git-archive.
if ("${GIT_SHA1}" STREQUAL "GITDIR-NOTFOUND")
    file(READ ${CMAKE_SOURCE_DIR}/.tag sha1_candidate)
    string(REPLACE "\n" "" sha1_candidate ${sha1_candidate})
    if (NOT ${sha1_candidate} STREQUAL "$Format:%H$")
        message("${sha1_candidate}")
        set (GIT_SHA1 "${sha1_candidate}")
    endif()
endif()
message(STATUS "GIT_SHA1 ${GIT_SHA1}")

set(SYSCONFDIR ${SYSCONF_INSTALL_DIR})
set(DATADIR ${DATA_INSTALL_DIR})
if(WIN32)
set(DATADIR "share")
endif(WIN32)
set(SHAREDIR ${DATADIR})

#####
## handle BUILD_OWNCLOUD_OSX_BUNDLE
# BUILD_OWNCLOUD_OSX_BUNDLE was not initialized OR set to true on OSX
if(APPLE AND (NOT DEFINED BUILD_OWNCLOUD_OSX_BUNDLE OR BUILD_OWNCLOUD_OSX_BUNDLE))
    set(BUILD_OWNCLOUD_OSX_BUNDLE ON)
    set(OWNCLOUD_OSX_BUNDLE "${APPLICATION_EXECUTABLE}.app")
    set(LIB_INSTALL_DIR "${APPLICATION_EXECUTABLE}.app/Contents/MacOS")
    set(BIN_INSTALL_DIR "${APPLICATION_EXECUTABLE}.app/Contents/MacOS")

# BUILD_OWNCLOUD_OSX_BUNDLE was disabled on OSX
elseif(APPLE AND NOT BUILD_OWNCLOUD_OSX_BUNDLE)
    message(FATAL_ERROR "Building in non-bundle mode on OSX is currently not supported. Comment this error out if you want to work on/test it.")

# any other platform
else()
    set(BUILD_OWNCLOUD_OSX_BUNDLE OFF)
endif()
#####

# this option removes Http authentication, keychain, shibboleth etc and is intended for
# external authentication mechanisms
option(TOKEN_AUTH_ONLY "TOKEN_AUTH_ONLY" OFF)
if(TOKEN_AUTH_ONLY)
   message("Compiling with token authentication")
   add_definitions(-DTOKEN_AUTH_ONLY=1)
endif()

option(NO_MSG_HANDLER "Don't redirect QDebug outputs to the log window/file" OFF)
if(NO_MSG_HANDLER)
   add_definitions(-DNO_MSG_HANDLER=1)
endif()

# this option builds the shell integration
option(BUILD_SHELL_INTEGRATION "BUILD_SHELL_INTEGRATION" ON)

# this option builds/installs the generic shell integration icons
option(BUILD_SHELL_INTEGRATION_ICONS "BUILD_SHELL_INTEGRATION_ICONS" ON)

# this options builds the dolphin integration plugin
option(BUILD_SHELL_INTEGRATION_DOLPHIN "BUILD_SHELL_INTEGRATION_DOLPHIN" ON)

# this options builds the nautilus (like) integration plugins
option(BUILD_SHELL_INTEGRATION_NAUTILUS "BUILD_SHELL_INTEGRATION_NAUTILUS" ON)

# this option builds the client
option(BUILD_CLIENT "BUILD_CLIENT" ON)

# this option creates only libocsync and libowncloudsync (NOTE: BUILD_CLIENT needs to be on)
option(BUILD_LIBRARIES_ONLY "BUILD_LIBRARIES_ONLY" OFF)

# When this option is enabled, 5xx errors are not added to the blacklist
# Normally you don't want to enable this option because if a particular file
# triggers a bug on the server, you want the file to be blacklisted.
option(OWNCLOUD_5XX_NO_BLACKLIST "OWNCLOUD_5XX_NO_BLACKLIST" OFF)
if(OWNCLOUD_5XX_NO_BLACKLIST)
    add_definitions(-DOWNCLOUD_5XX_NO_BLACKLIST=1)
endif()

# When this option is enabled, a rename that is not allowed will be renamed back
# do the original as a restoration step.  Withut this option, the restoration will
# re-download the file instead.
# The default is off because we don't want to rename the files back behind the user's back
# Added for IL issue #550
option(OWNCLOUD_RESTORE_RENAME "OWNCLOUD_RESTORE_RENAME" OFF)
if(OWNCLOUD_RESTORE_RENAME)
    add_definitions(-DOWNCLOUD_RESTORE_RENAME=1)
endif()

# Disable shibboleth.
# So the client can be built without QtWebKit
option(NO_SHIBBOLETH "Build without Shibboleth support. Allow to build the client without QtWebKit" OFF)
if(NO_SHIBBOLETH)
   message("Compiling without shibboleth")
   add_definitions(-DNO_SHIBBOLETH=1)
endif()

if(APPLE)
  set( SOCKETAPI_TEAM_IDENTIFIER_PREFIX "" CACHE STRING "SocketApi prefix (including a following dot) that must match the codesign key's TeamIdentifier/Organizational Unit" )
endif()

if(BUILD_CLIENT)
    if(APPLE)
        find_package(Sparkle)
    endif(APPLE)

    if(UNIX)
        find_package(INotify REQUIRED)
    else()
        find_package(INotify)
    endif()
    find_package(Sphinx)
    find_package(PdfLatex)

    find_package(SQLite3 3.8.0 REQUIRED)
    # On some OS, we want to use our own, not the system sqlite
    if (USE_OUR_OWN_SQLITE3)
        include_directories(BEFORE ${SQLITE3_INCLUDE_DIR})
        if (WIN32)
            add_definitions(-DSQLITE_API=__declspec\(dllimport\))
        endif()
    endif()

   find_package(ZLIB)
endif()

if (NOT DEFINED APPLICATION_ICON_NAME)
    set(APPLICATION_ICON_NAME ${APPLICATION_SHORTNAME})
endif()

include(OwnCloudCPack.cmake)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)
if( WIN32 )
add_definitions( -D__USE_MINGW_ANSI_STDIO=1 )
add_definitions( -DNOMINMAX )
endif( WIN32 )

include(QtVersionAbstraction)
setup_qt()
if (${Qt5Core_VERSION_MAJOR} EQUAL "5")
    if (${Qt5Core_VERSION_MINOR} EQUAL "6" OR ${Qt5Core_VERSION_MINOR} GREATER 6)
    else()
      message(STATUS "If possible compile me with Qt 5.6 or higher.")
    endif()
    if (${Qt5Core_VERSION_MINOR} EQUAL "9" OR ${Qt5Core_VERSION_MINOR} GREATER 9)
    else()
      message(STATUS "For HTTP2 use Qt 5.9.2 or higher.")
    endif()
endif()
message("Qt ${Qt5Core_VERSION} at ${Qt5Core_INCLUDE_DIRS}")

if (APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

# Handle Translations, pick all client_* files from trans directory.
file( GLOB TRANS_FILES ${CMAKE_SOURCE_DIR}/translations/client_*.ts)
set(TRANSLATIONS ${TRANS_FILES})

# Make sure we set this before recursing into child folders.
set(WITH_TESTING ${UNIT_TESTING})

if(BUILD_CLIENT)
    add_subdirectory(src)
    if(NOT BUILD_LIBRARIES_ONLY)
        add_subdirectory(doc)
        add_subdirectory(doc/dev)
        if(IS_DIRECTORY admin)
            add_subdirectory(admin)
        endif(IS_DIRECTORY admin)
    endif(NOT BUILD_LIBRARIES_ONLY)
endif()

if(BUILD_SHELL_INTEGRATION)
    add_subdirectory(shell_integration)
endif()

if(UNIT_TESTING)
    include(CTest)
    enable_testing()
    add_subdirectory(test)
endif(UNIT_TESTING)

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)

if(BUILD_OWNCLOUD_OSX_BUNDLE)
    install(FILES sync-exclude.lst DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources/)
    configure_file(sync-exclude.lst bin/${OWNCLOUD_OSX_BUNDLE}/Contents/Resources/sync-exclude.lst COPYONLY)
elseif(BUILD_CLIENT)
    install( FILES sync-exclude.lst DESTINATION ${SYSCONFDIR}/${APPLICATION_SHORTNAME} )
    configure_file(sync-exclude.lst bin/sync-exclude.lst COPYONLY)
endif()
