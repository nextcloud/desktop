project(libsync)
include(DefinePlatformDefaults)

set(CMAKE_AUTOMOC TRUE)

if ( APPLE )
    list(APPEND OS_SPECIFIC_LINK_LIBRARIES
         /System/Library/Frameworks/CoreServices.framework
         /System/Library/Frameworks/Foundation.framework
         /System/Library/Frameworks/AppKit.framework
    )
endif()

set(libsync_SRCS
    account.h
    account.cpp
    pushnotifications.h
    pushnotifications.cpp
    wordlist.h
    wordlist.cpp
    bandwidthmanager.h
    bandwidthmanager.cpp
    capabilities.h
    capabilities.cpp
    clientproxy.h
    clientproxy.cpp
    cookiejar.h
    cookiejar.cpp
    discovery.h
    discovery.cpp
    discoveryphase.h
    discoveryphase.cpp
    encryptfolderjob.h
    encryptfolderjob.cpp
    filesystem.h
    filesystem.cpp
    httplogger.h
    httplogger.cpp
    logger.h
    logger.cpp
    accessmanager.h
    accessmanager.cpp
    configfile.h
    configfile.cpp
    abstractnetworkjob.h
    abstractnetworkjob.cpp
    networkjobs.h
    networkjobs.cpp
    iconjob.h
    iconjob.cpp
    owncloudpropagator.h
    owncloudpropagator.cpp
    nextcloudtheme.h
    nextcloudtheme.cpp
    abstractpropagateremotedeleteencrypted.h
    abstractpropagateremotedeleteencrypted.cpp
    deletejob.h
    deletejob.cpp
    progressdispatcher.h
    progressdispatcher.cpp
    propagatorjobs.h
    propagatorjobs.cpp
    propagatedownload.h
    propagatedownload.cpp
    propagateupload.h
    propagateupload.cpp
    propagateuploadv1.cpp
    propagateuploadng.cpp
    bulkpropagatorjob.h
    bulkpropagatorjob.cpp
    putmultifilejob.h
    putmultifilejob.cpp
    propagateremotedelete.h
    propagateremotedelete.cpp
    propagateremotedeleteencrypted.h
    propagateremotedeleteencrypted.cpp
    propagateremotedeleteencryptedrootfolder.h
    propagateremotedeleteencryptedrootfolder.cpp
    propagateremotemove.h
    propagateremotemove.cpp
    propagateremotemkdir.h
    propagateremotemkdir.cpp
    propagateuploadencrypted.h
    propagateuploadencrypted.cpp
    propagatedownloadencrypted.h
    propagatedownloadencrypted.cpp
    syncengine.h
    syncengine.cpp
    syncfileitem.h
    syncfileitem.cpp
    syncfilestatustracker.h
    syncfilestatustracker.cpp
    localdiscoverytracker.h
    localdiscoverytracker.cpp
    syncresult.h
    syncresult.cpp
    syncoptions.h
    syncoptions.cpp
    theme.h
    theme.cpp
    clientsideencryption.h
    clientsideencryption.cpp
    clientsideencryptionjobs.h
    clientsideencryptionjobs.cpp
    datetimeprovider.h
    datetimeprovider.cpp
    ocsuserstatusconnector.h
    ocsuserstatusconnector.cpp
    userstatusconnector.h
    userstatusconnector.cpp
    ocsprofileconnector.h
    ocsprofileconnector.cpp
    lockfilejobs.h
    lockfilejobs.cpp
    creds/dummycredentials.h
    creds/dummycredentials.cpp
    creds/abstractcredentials.h
    creds/abstractcredentials.cpp
    creds/credentialscommon.h
    creds/credentialscommon.cpp
    creds/keychainchunk.h
    creds/keychainchunk.cpp
)

if (WIN32)
    # to fix warnings from ntstatus.h
    add_definitions(-DUMDF_USING_NTSTATUS)
endif()

if(TOKEN_AUTH_ONLY)
    set (libsync_SRCS
        ${libsync_SRCS}
        creds/tokencredentials.h
        creds/tokencredentials.cpp)
else()
    set (libsync_SRCS
        ${libsync_SRCS}
        creds/httpcredentials.h
        creds/httpcredentials.cpp)
endif()

# These headers are installed for libowncloudsync to be used by 3rd party apps
set(owncloudsync_HEADERS
    account.h
    syncengine.h
    configfile.h
    networkjobs.h
    progressdispatcher.h
    syncfileitem.h
    syncresult.h
)

set(creds_HEADERS
    creds/abstractcredentials.h
    creds/httpcredentials.h
)

IF (NOT APPLE)
    INSTALL(
        FILES ${owncloudsync_HEADERS}
	DESTINATION ${INCLUDE_INSTALL_DIR}/nextcloudsync/mirall
    )
    INSTALL(
        FILES ${creds_HEADERS}
	DESTINATION ${INCLUDE_INSTALL_DIR}/nextcloudsync/creds
    )
ENDIF(NOT APPLE)

find_package(Qt5 REQUIRED COMPONENTS WebSockets)

add_library(nextcloudsync SHARED ${libsync_SRCS})
add_library(Nextcloud::sync ALIAS nextcloudsync)

target_link_libraries(nextcloudsync
  PUBLIC
  Nextcloud::csync
  OpenSSL::Crypto
  OpenSSL::SSL
  ${OS_SPECIFIC_LINK_LIBRARIES}
  Qt5::Core
  Qt5::Network
  Qt5::WebSockets
)

if (NOT TOKEN_AUTH_ONLY)
    find_package(Qt5 REQUIRED COMPONENTS Widgets Svg)
    target_link_libraries(nextcloudsync PUBLIC Qt5::Widgets Qt5::Svg qt5keychain)
endif()

if(Inotify_FOUND)
    target_include_directories(nextcloudsync PRIVATE ${Inotify_INCLUDE_DIRS})
    target_link_libraries(nextcloudsync PUBLIC ${Inotify_LIBRARIES})
endif()

GENERATE_EXPORT_HEADER( nextcloudsync
	BASE_NAME nextcloudsync
	EXPORT_MACRO_NAME OWNCLOUDSYNC_EXPORT
        EXPORT_FILE_NAME owncloudlib.h
	STATIC_DEFINE OWNCLOUD_BUILT_AS_STATIC
)

target_include_directories(nextcloudsync PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set_target_properties(
  nextcloudsync
    PROPERTIES
      VERSION
        ${MIRALL_VERSION}
      SOVERSION
        ${MIRALL_SOVERSION}
      RUNTIME_OUTPUT_DIRECTORY
        ${BIN_OUTPUT_DIRECTORY}
      LIBRARY_OUTPUT_NAME
        ${APPLICATION_EXECUTABLE}sync
      RUNTIME_OUTPUT_NAME
        ${APPLICATION_EXECUTABLE}sync
)

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE)
    install(TARGETS nextcloudsync
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
else()
    install(TARGETS nextcloudsync DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/MacOS)
endif()


add_subdirectory(vfs)
