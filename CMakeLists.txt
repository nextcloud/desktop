cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 14)

include(VERSION.cmake)
project(client LANGUAGES CXX C VERSION ${MIRALL_VERSION_MAJOR}.${MIRALL_VERSION_MINOR}.${MIRALL_VERSION_PATCH})
include(FeatureSummary)
find_package(ECM 5.50.0  NO_MODULE)
set_package_properties(ECM PROPERTIES TYPE REQUIRED DESCRIPTION "Extra CMake Modules." URL "https://projects.kde.org/projects/kdesupport/extra-cmake-modules")
feature_summary(WHAT REQUIRED_PACKAGES_NOT_FOUND FATAL_ON_MISSING_REQUIRED_PACKAGES)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${ECM_MODULE_PATH} ${CMAKE_MODULE_PATH})

# disable pointless warning in KDECMakeSettings
set(APPLE_SUPPRESS_X11_WARNING ON)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(ECMMarkNonGuiExecutable)

include(KDECompilerSettings NO_POLICY_SCOPE)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Qt5 REQUIRED COMPONENTS Core)

if(UNIT_TESTING)
    message(DEPRECATION "Setting UNIT_TESTING is deprecated please use BUILD_TESTING")
    set(BUILD_TESTING TRUE)
endif()
include(CTest)


include(OCConfigPluginDir)
include(OCGenerateTheme)

if (EXISTS "${PROJECT_SOURCE_DIR}/branding")
    set(OEM_THEME_DIR "${PROJECT_SOURCE_DIR}/branding" CACHE STRING "The directory containing a custom theme")
else()
    set(OEM_THEME_DIR "${PROJECT_SOURCE_DIR}" CACHE STRING "Define directory containing a custom theme")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/THEME.cmake")

set(synclib_NAME "${APPLICATION_EXECUTABLE}sync")


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

include(GenerateExportHeader)

include(GetGitRevisionDescription)

get_git_head_revision(GIT_REFSPEC GIT_SHA1)

add_definitions(
    -DQT_USE_QSTRINGBUILDER
    -DQT_MESSAGELOGCONTEXT #enable function name and line number in debug output
    -DQT_DEPRECATED_WARNINGS
    -DQT_NO_FOREACH
)

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

# this option builds the client documentation
option(BUILD_DOCS "BUILD_DOCS" OFF)

# this option creates only libocsync and libowncloudsync (NOTE: BUILD_CLIENT needs to be on)
option(BUILD_LIBRARIES_ONLY "BUILD_LIBRARIES_ONLY" OFF)

# build the GUI component, when disabled only owncloudcmd is built
option(BUILD_GUI "BUILD_GUI" ON)

# specify additional vfs plugins
set(VIRTUAL_FILE_SYSTEM_PLUGINS suffix win CACHE STRING "Name of internal plugin in src/libsync/vfs or the locations of virtual file plugins")

# When this option is enabled, 5xx errors are not added to the blacklist
# Normally you don't want to enable this option because if a particular file
# triggers a bug on the server, you want the file to be blacklisted.
option(OWNCLOUD_5XX_NO_BLACKLIST "OWNCLOUD_5XX_NO_BLACKLIST" OFF)
if(OWNCLOUD_5XX_NO_BLACKLIST)
    add_definitions(-DOWNCLOUD_5XX_NO_BLACKLIST=1)
endif()

if(APPLE)
  set( SOCKETAPI_TEAM_IDENTIFIER_PREFIX "" CACHE STRING "SocketApi prefix (including a following dot) that must match the codesign key's TeamIdentifier/Organizational Unit" )
endif()

if(BUILD_CLIENT)
    OPTION(GUI_TESTING "Build with gui introspection features of socket api" OFF)

    if(APPLE)
        find_package(Sparkle REQUIRED)
    endif(APPLE)

    if(UNIX AND NOT APPLE)
        find_package(Inotify REQUIRED)
    endif()

    find_package(ZLIB REQUIRED)
    find_package(SQLite3 3.9.0 REQUIRED)
endif()

if (NOT DEFINED APPLICATION_ICON_NAME)
    set(APPLICATION_ICON_NAME "${APPLICATION_SHORTNAME}")
endif()

if( WIN32 )
    add_definitions( -D__USE_MINGW_ANSI_STDIO=1 )
    add_definitions( -DNOMINMAX )
    # Get APIs from from Vista onwards.
    add_definitions(-D_WIN32_WINNT=0x0600)
    add_definitions(-DWINVER=0x0600)
    add_definitions(-DNTDDI_VERSION=0x0A000003)
endif()

# Handle Translations, pick all client_* files from trans directory.
file( GLOB TRANS_FILES ${CMAKE_SOURCE_DIR}/translations/client_*.ts)
set(TRANSLATIONS ${TRANS_FILES})


file(GLOB_RECURSE OWNCLOUD_ICONS "${OEM_THEME_DIR}/theme/colored/*-${APPLICATION_ICON_NAME}-icon.png")
if (NOT OWNCLOUD_ICONS)
    generate_legacy_icons(${OEM_THEME_DIR}/theme OWNCLOUD_ICONS)
endif()
MESSAGE(STATUS "OWNCLOUD_ICONS: ${APPLICATION_ICON_NAME}: ${OWNCLOUD_ICONS}")

if(APPLE)
    list(APPEND OWNCLOUD_BUNDLED_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/sync-exclude.lst")
elseif(BUILD_CLIENT)
    configure_file(sync-exclude.lst bin/sync-exclude.lst COPYONLY)
    if(WIN32)
        install(FILES sync-exclude.lst DESTINATION bin)
    else()
        install(FILES sync-exclude.lst DESTINATION ${SYSCONFDIR}/${APPLICATION_SHORTNAME})
    endif()
endif()

if(BUILD_CLIENT)
    add_subdirectory(src)
    if(NOT BUILD_LIBRARIES_ONLY)
        if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/admin)
            add_subdirectory(admin)
        endif(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/admin)
    endif(NOT BUILD_LIBRARIES_ONLY)
endif()

if(BUILD_DOCS)
    add_subdirectory(docs)
endif()


if(BUILD_SHELL_INTEGRATION)
    add_subdirectory(shell_integration)
endif()

if(BUILD_TESTING)
    add_subdirectory(test)
endif()

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
