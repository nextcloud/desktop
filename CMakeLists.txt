
cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.8.0)
if(POLICY CMP0020)
  cmake_policy(SET CMP0020 NEW)
endif()

project(client)

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

if (NOT DEFINED APPLICATION_SHORTNAME)
     set ( APPLICATION_SHORTNAME ${APPLICATION_NAME} )
endif()

# For usage in XML files we preprocess
string(REPLACE "&" "&amp;" APPLICATION_NAME_XML_ESCAPED "${APPLICATION_NAME}")
string(REPLACE "<" "&lt;" APPLICATION_NAME_XML_ESCAPED "${APPLICATION_NAME_XML_ESCAPED}")
string(REPLACE ">" "&gt;" APPLICATION_NAME_XML_ESCAPED "${APPLICATION_NAME_XML_ESCAPED}")

set(PACKAGE "${APPLICATION_SHORTNAME}-client")
set( CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules )

if(NOT CRASHREPORTER_EXECUTABLE)
    set(CRASHREPORTER_EXECUTABLE "${APPLICATION_EXECUTABLE}_crash_reporter")
endif()

include(Warnings)

include(${CMAKE_SOURCE_DIR}/VERSION.cmake)
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})

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
if (${GIT_SHA1} STREQUAL "GITDIR-NOTFOUND")
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

# this option creates only libocsync and libowncloudsync
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

find_package(OpenSSL 1.0.0 REQUIRED)

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

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

configure_file(test/test_journal.db "${CMAKE_BINARY_DIR}/test/test_journal.db" COPYONLY)

include(OwnCloudCPack.cmake)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)
if( WIN32 )
add_definitions( -D__USE_MINGW_ANSI_STDIO=1 )
add_definitions( -DNOMINMAX )
endif( WIN32 )

# Handle Translations, pick all client_* files from trans directory.
file( GLOB TRANS_FILES ${CMAKE_SOURCE_DIR}/translations/client_*.ts)
set(TRANSLATIONS ${TRANS_FILES})

add_subdirectory(csync)
add_subdirectory(src)
if(NOT BUILD_LIBRARIES_ONLY)
add_subdirectory(shell_integration)
add_subdirectory(doc)
add_subdirectory(doc/dev)
add_subdirectory(admin)
endif(NOT BUILD_LIBRARIES_ONLY)

if(UNIT_TESTING)
    include(CTest)
    enable_testing()
    add_subdirectory(test)
endif(UNIT_TESTING)

if(BUILD_OWNCLOUD_OSX_BUNDLE)
    install(FILES sync-exclude.lst DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources/)
    configure_file(sync-exclude.lst bin/${OWNCLOUD_OSX_BUNDLE}/Contents/Resources/sync-exclude.lst COPYONLY)
else()
    install( FILES sync-exclude.lst DESTINATION ${SYSCONFDIR}/${APPLICATION_SHORTNAME} )
    configure_file(sync-exclude.lst bin/sync-exclude.lst COPYONLY)
endif()
