
cmake_minimum_required(VERSION 2.6)
project(mirall)
set(PACKAGE "mirall")
set( CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules )

if ( EXISTS ${CMAKE_SOURCE_DIR}/OEM.cmake )
     include ( ${CMAKE_SOURCE_DIR}/OEM.cmake )
else ()
     include ( ${CMAKE_SOURCE_DIR}/OWNCLOUD.cmake )
endif()

include(${CMAKE_SOURCE_DIR}/VERSION.cmake)
configure_file( ${CMAKE_SOURCE_DIR}/src/mirall/version.h.in "${CMAKE_CURRENT_BINARY_DIR}/src/mirall/version.h" )
include_directories(BEFORE "${CMAKE_CURRENT_BINARY_DIR}/src/mirall/")

include(GNUInstallDirs)

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

## stupid, we should upstream this
if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr" AND NOT CMAKE_INSTALL_SYSCONFDIR)
    set(CMAKE_INSTALL_SYSCONFDIR "/etc")
endif()

#####
## handle BUILD_OWNCLOUD_OSX_BUNDLE
# BUILD_OWNCLOUD_OSX_BUNDLE was not initialized OR set to true on OSX
if(APPLE AND (NOT DEFINED BUILD_OWNCLOUD_OSX_BUNDLE OR BUILD_OWNCLOUD_OSX_BUNDLE))
    set(BUILD_OWNCLOUD_OSX_BUNDLE ON)
    set(OWNCLOUD_OSX_BUNDLE "${APPLICATION_EXECUTABLE}.app")

# BUILD_OWNCLOUD_OSX_BUNDLE was disabled on OSX
elseif(APPLE AND NOT BUILD_OWNCLOUD_OSX_BUNDLE)
    message(FATAL_ERROR "Building in non-bundle mode on OSX is currently not supported. Comment this error out if you want to work on/test it.")

# any other platform
else()
    set(BUILD_OWNCLOUD_OSX_BUNDLE OFF)
endif()
#####

#### find libs
find_package(Qt4 4.6.0 COMPONENTS QtCore QtGui QtXml QtNetwork QtTest REQUIRED )
find_package(Csync)
if(UNIX)
find_package(INotify REQUIRED)
else()
find_package(INotify)
endif()
find_package(Sphinx)
find_package(PdfLatex)
find_package(QtKeychain)

set(WITH_CSYNC CSYNC_FOUND)
set(WITH_QTKEYCHAIN ${QTKEYCHAIN_FOUND})
set(USE_INOTIFY ${INOTIFY_FOUND})

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(CPACK_SOURCE_IGNORE_FILES
  # hidden files
  "/\\\\..+$"
  # temporary files
  "\\\\.swp$"
  # backup files
  "~$"
  # others
  "\\\\.#"
  "/#"
  "/build/"
  "/_build/"
  # used before
  "\\\\.o$"
  "\\\\.lo$"
  "\\\\.la$"
  "Makefile\\\\.in$"
)

include(OwnCloudCPack.cmake)

#
# This cmake builds two targets (aka apps), mirall and owncloud. For the owncloud
# target, OWNCLOUD_CLIENT needs to be a compile flag. It is set in src/CMakeLists.txt
# but if that fails because cmake is too old, uncomment this here if you want to build
# owncloud.
# add_definitions(-DOWNCLOUD_CLIENT)
add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)

# Handle Translations, pick all mirall_* files from trans directory.
file( GLOB TRANS_FILES ${CMAKE_SOURCE_DIR}/translations/mirall_*.ts)
set(TRANSLATIONS ${TRANS_FILES})

add_subdirectory(src)
add_subdirectory(doc)

if(UNIT_TESTING)
    include(CTest)
    enable_testing()
    add_subdirectory(test)
endif(UNIT_TESTING)

if(BUILD_OWNCLOUD_OSX_BUNDLE)
    configure_file(sync-exclude.lst ${OWNCLOUD_OSX_BUNDLE}/Contents/Resources/sync-exclude.lst COPYONLY)
else()
    install( FILES sync-exclude.lst DESTINATION ${CMAKE_INSTALL_SYSCONFDIR} )
endif()
