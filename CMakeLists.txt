cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0071 NEW) # Enable use of QtQuick compiler/generated code

project(client)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum OSX deployment version")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED 17)

include(FeatureSummary)

find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if (CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE} -checks=-*,modernize-use-auto,modernize-use-using,modernize-use-nodiscard,modernize-use-nullptr,modernize-use-override,cppcoreguidelines-pro-type-static-cast-downcast,modernize-use-default-member-init,cppcoreguidelines-pro-type-member-init,cppcoreguidelines-init-variables)
endif()

set(CMAKE_XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES)

set(BIN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(${CMAKE_SOURCE_DIR}/NEXTCLOUD.cmake)

# CfAPI Shell Extensions
set( CFAPI_SHELL_EXTENSIONS_LIB_NAME CfApiShellExtensions )
    
set( CFAPI_SHELLEXT_APPID_REG "{E314A650-DCA4-416E-974E-18EA37C213EA}")
set( CFAPI_SHELLEXT_APPID_DISPLAY_NAME "${APPLICATION_NAME} CfApi Shell Extensions" )

set( CFAPI_SHELLEXT_CUSTOM_STATE_HANDLER_CLASS_ID "1E62D59A-6EA4-476C-B707-4A32E88ED822" )
set( CFAPI_SHELLEXT_CUSTOM_STATE_HANDLER_CLASS_ID_REG "{${CFAPI_SHELLEXT_CUSTOM_STATE_HANDLER_CLASS_ID}}" )
set( CFAPI_SHELLEXT_CUSTOM_STATE_HANDLER_DISPLAY_NAME "${APPLICATION_NAME} Custom State Handler" )

set( CFAPI_SHELLEXT_THUMBNAIL_HANDLER_CLASS_ID "6FF9B5B6-389F-444A-9FDD-A286C36EA079" )
set( CFAPI_SHELLEXT_THUMBNAIL_HANDLER_CLASS_ID_REG "{${CFAPI_SHELLEXT_THUMBNAIL_HANDLER_CLASS_ID}}" )
set( CFAPI_SHELLEXT_THUMBNAIL_HANDLER_DISPLAY_NAME "${APPLICATION_NAME} Thumbnail Handler" )

# URI Handler Scheme for Local File Editing
set( APPLICATION_URI_HANDLER_SCHEME "nc")

# Default suffix if the theme doesn't define one
if(NOT DEFINED APPLICATION_VIRTUALFILE_SUFFIX)
    set(APPLICATION_VIRTUALFILE_SUFFIX "${APPLICATION_SHORTNAME}_virtual" CACHE STRING "Virtual file suffix (not including the .)")
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

include(ECMCoverageOption)

if(NOT CRASHREPORTER_EXECUTABLE)
    set(CRASHREPORTER_EXECUTABLE "${APPLICATION_EXECUTABLE}_crash_reporter")
endif()

include(Warnings)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  add_compile_options(-fdiagnostics-color=always)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  add_compile_options(-fcolor-diagnostics)
endif()

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

include(GNUInstallDirs)
include(DefineInstallationPaths)
include(GenerateExportHeader)

include(GetGitRevisionDescription)

get_git_head_revision(GIT_REFSPEC GIT_SHA1)

add_definitions(
    -DQT_DISABLE_DEPRECATED_BEFORE=0x000000
    -DQT_USE_QSTRINGBUILDER
    -DQT_MESSAGELOGCONTEXT #enable function name and line number in debug output
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
set(SHAREDIR ${CMAKE_INSTALL_FULL_DATADIR})

# Build MacOS app bundle if wished
if(APPLE AND BUILD_OWNCLOUD_OSX_BUNDLE)
    message(STATUS "Build MacOS app bundle")
    set(OWNCLOUD_OSX_BUNDLE "${APPLICATION_NAME}.app")
    set(LIB_INSTALL_DIR "${APPLICATION_NAME}.app/Contents/MacOS")
    set(BIN_INSTALL_DIR "${APPLICATION_NAME}.app/Contents/MacOS")
    add_definitions(-DBUILD_OWNCLOUD_OSX_BUNDLE)
endif()


option(QUICK_COMPILER "Use QtQuick compiler to improve performance" OFF)

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

if(BUILD_UPDATER)
   message("Compiling with updater")
else()
   message("Compiling without updater")
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

# build the GUI component, when disabled only nextcloudcmd is built
option(BUILD_GUI "BUILD_GUI" ON)

# build the tests
option(BUILD_TESTING "BUILD_TESTING" ON)

option(ENABLE_CLANG_TIDY "ENABLE_CLANG_TIDY" OFF)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if (CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE} -checks=-*,modernize-use-auto,modernize-use-using,modernize-use-nodiscard,modernize-use-nullptr,modernize-use-override,cppcoreguidelines-pro-type-static-cast-downcast,modernize-use-default-member-init,cppcoreguidelines-pro-type-member-init,cppcoreguidelines-init-variables)
    endif()
else()
    unset(CLANG_TIDY_EXE)
    unset(CMAKE_CXX_CLANG_TIDY)
endif()

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

    if(APPLE AND BUILD_UPDATER)
        find_package(Sparkle)
    endif()

    if(UNIX AND NOT APPLE)
        find_package(Inotify REQUIRED)
    endif()
    find_package(Sphinx)
    find_package(PdfLatex)
    find_package(OpenSSL 1.1 REQUIRED )

   find_package(ZLIB REQUIRED)
   find_package(SQLite3 3.9.0 REQUIRED)

   if(NOT WIN32 AND NOT APPLE)
      find_package(PkgConfig REQUIRED)
      pkg_check_modules(CLOUDPROVIDERS cloudproviders IMPORTED_TARGET)

      if(CLOUDPROVIDERS_FOUND)
        pkg_check_modules(DBUS-1 REQUIRED dbus-1 IMPORTED_TARGET)
        pkg_check_modules(GIO REQUIRED gio-2.0 IMPORTED_TARGET)
        pkg_check_modules(GLIB2 REQUIRED glib-2.0 IMPORTED_TARGET)
      endif()
   endif()
endif()

if (APPLE)
    option(BUILD_WITH_WEBENGINE "BUILD_WITH_WEBENGINE" OFF)
else ()
    option(BUILD_WITH_WEBENGINE "BUILD_WITH_WEBENGINE" ON)
endif()

if (NOT DEFINED APPLICATION_ICON_NAME)
    set(APPLICATION_ICON_NAME ${APPLICATION_SHORTNAME})
endif()

include(NextcloudCPack.cmake)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)
if( WIN32 )
add_definitions( -D__USE_MINGW_ANSI_STDIO=1 )
add_definitions( -DNOMINMAX )
# Get APIs from from Vista onwards.
add_definitions(-D_WIN32_WINNT=0x0601)
add_definitions(-DWINVER=0x0601)
add_definitions(-DNTDDI_VERSION=0x0A000004)
    if( MSVC )
    # Use automatic overload for suitable CRT safe-functions
    # See https://docs.microsoft.com/de-de/cpp/c-runtime-library/security-features-in-the-crt?view=vs-2019
    add_definitions( -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1 )
    # Also: Disable compiler warnings because we don't use Windows CRT safe-functions explicitly and don't intend to
    # as this is a pure cross-platform source the only alternative would be a ton of ifdefs with calls to the _s version
    add_definitions( -D_CRT_SECURE_NO_WARNINGS )
    endif( MSVC )
endif( WIN32 )

if (APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

# Handle Translations, pick all client_* files from trans directory.
file( GLOB TRANS_FILES ${CMAKE_SOURCE_DIR}/translations/client_*.ts)
set(TRANSLATIONS ${TRANS_FILES})

if(BUILD_CLIENT)
    add_subdirectory(src)
    if(NOT BUILD_LIBRARIES_ONLY)
        add_subdirectory(man)
        add_subdirectory(doc)
        add_subdirectory(doc/dev)
        if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/admin)
            add_subdirectory(admin)
        endif(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/admin)
    endif(NOT BUILD_LIBRARIES_ONLY)
endif()

if(BUILD_SHELL_INTEGRATION)
    add_subdirectory(shell_integration)
endif()

if(BUILD_TESTING)
    include(CTest)
    enable_testing()
    add_subdirectory(test)
endif()

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)

if(BUILD_OWNCLOUD_OSX_BUNDLE)
    install(FILES sync-exclude.lst DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources/)
    configure_file(sync-exclude.lst bin/${OWNCLOUD_OSX_BUNDLE}/Contents/Resources/sync-exclude.lst COPYONLY)
elseif(BUILD_CLIENT)
    install( FILES sync-exclude.lst DESTINATION ${SYSCONFDIR}/${APPLICATION_SHORTNAME} )
    configure_file(sync-exclude.lst bin/sync-exclude.lst COPYONLY)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES INCLUDE_QUIET_PACKAGES)
