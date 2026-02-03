# SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2014 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later
project(libsync)
find_package(KF6Archive REQUIRED)
include(DefinePlatformDefaults)

set(CMAKE_AUTOMOC TRUE)

if ( APPLE )
    list(APPEND OS_SPECIFIC_LINK_LIBRARIES
         /System/Library/Frameworks/CoreServices.framework
         /System/Library/Frameworks/Foundation.framework
         /System/Library/Frameworks/AppKit.framework
    )
endif()

if (WIN32)
  list(APPEND OS_SPECIFIC_LINK_LIBRARIES
    Crypt32
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
    clientstatusreporting.h
    clientstatusreporting.cpp
    clientstatusreportingcommon.h
    clientstatusreportingcommon.cpp
    clientstatusreportingdatabase.h
    clientstatusreportingdatabase.cpp
    clientstatusreportingnetwork.h
    clientstatusreportingnetwork.cpp
    clientstatusreportingrecord.h
    cookiejar.h
    cookiejar.cpp
    discovery.h
    discovery.cpp
    discoveryphase.h
    discoveryphase.cpp
    encryptfolderjob.h
    encryptfolderjob.cpp
    encryptedfoldermetadatahandler.h
    encryptedfoldermetadatahandler.cpp
    filesystem.h
    filesystem.cpp
    helpers.cpp
    httplogger.h
    httplogger.cpp
    updatechannel.h
    updatechannel.cpp
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
    basepropagateremotedeleteencrypted.h
    basepropagateremotedeleteencrypted.cpp
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
    bulkpropagatordownloadjob.h
    bulkpropagatordownloadjob.cpp
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
    updatee2eefoldermetadatajob.h
    updatee2eefoldermetadatajob.cpp
    updatemigratede2eemetadatajob.h
    updatemigratede2eemetadatajob.cpp
    updatee2eefolderusersmetadatajob.h
    updatee2eefolderusersmetadatajob.cpp
    clientsideencryption.h
    clientsideencryption.cpp
    clientsideencryptionjobs.h
    clientsideencryptionjobs.cpp
    clientsideencryptionprimitives.h
    clientsideencryptionprimitives.cpp
    clientsideencryptiontokenselector.h
    clientsideencryptiontokenselector.cpp
    datetimeprovider.h
    datetimeprovider.cpp
    rootencryptedfolderinfo.h
    rootencryptedfolderinfo.cpp
    foldermetadata.h
    foldermetadata.cpp
    ocsassistantconnector.h
    ocsassistantconnector.cpp
    ocsuserstatusconnector.h
    ocsuserstatusconnector.cpp
    rootencryptedfolderinfo.cpp
    rootencryptedfolderinfo.h
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
    caseclashconflictsolver.h
    caseclashconflictsolver.cpp
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

find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS WebSockets Xml Sql Gui Svg Widgets)

add_library(nextcloudsync SHARED ${libsync_SRCS})
add_library(Nextcloud::sync ALIAS nextcloudsync)

target_link_libraries(nextcloudsync
  PUBLIC
  Nextcloud::csync
  OpenSSL::Crypto
  OpenSSL::SSL
  PkgConfig::OPENSC-LIBP11
  ${OS_SPECIFIC_LINK_LIBRARIES}
  Qt::Core
  Qt::Network
  Qt::WebSockets
  Qt::Xml
  Qt::Sql
  Qt::Gui
  Qt::Svg
  Qt::Widgets
  KF6::Archive
  Qt::Core5Compat
)

target_compile_features(nextcloudsync
    PRIVATE
        cxx_std_17
)

target_compile_definitions(nextcloudsync PRIVATE OPENSSL_SUPPRESS_DEPRECATED)

if (NOT TOKEN_AUTH_ONLY)
    find_package(Qt${QT_MAJOR_VERSION} COMPONENTS REQUIRED Widgets Svg)
    target_link_libraries(nextcloudsync PUBLIC Qt::Widgets Qt::Svg Qt${QT_MAJOR_VERSION}Keychain::Qt${QT_MAJOR_VERSION}Keychain)
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
      ARCHIVE_OUTPUT_NAME
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

if (WIN32)
    install(FILES $<TARGET_PDB_FILE:nextcloudsync> DESTINATION bin OPTIONAL)
endif()

add_subdirectory(vfs)
