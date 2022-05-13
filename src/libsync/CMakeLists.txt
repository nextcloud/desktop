set(LibreGraphAPIVersion v0.13.2)
# TODO: pass version to find_package once LibreGraphAPI provides versioned releases
find_package(LibreGraphAPI QUIET)
if (NOT TARGET OpenAPI::LibreGraphAPI)
    message(WARNING "Failed to find system OpenAPI::LibreGraphAPI: fetching ${LibreGraphAPIVersion}")
    include(FetchContent)
    FetchContent_Populate(LibreGraphAPISrc
                        QUIET
                        GIT_REPOSITORY https://github.com/owncloud/libre-graph-api-cpp-qt-client.git
                        GIT_TAG ${LibreGraphAPIVersion})
    add_subdirectory(${libregraphapisrc_SOURCE_DIR}/client ${libregraphapisrc_BINARY_DIR}/client EXCLUDE_FROM_ALL)
endif()

set(libsync_SRCS
    account.cpp
    bandwidthmanager.cpp
    capabilities.cpp
    cookiejar.cpp
    discovery.cpp
    discoveryphase.cpp
    filesystem.cpp
    httplogger.cpp
    jobqueue.cpp
    logger.cpp
    accessmanager.cpp
    configfile.cpp
    abstractnetworkjob.cpp
    networkjobs.cpp
    owncloudpropagator.cpp
    owncloudtheme.cpp
    progressdispatcher.cpp
    propagatorjobs.cpp
    propagatedownload.cpp
    propagateupload.cpp
    propagateuploadv1.cpp
    propagateuploadng.cpp
    propagateuploadtus.cpp
    propagateremotedelete.cpp
    propagateremotemove.cpp
    propagateremotemkdir.cpp
    syncengine.cpp
    syncfileitem.cpp
    syncfilestatustracker.cpp
    localdiscoverytracker.cpp
    syncresult.cpp
    syncoptions.cpp
    theme.cpp
    creds/jobs/determineuserjobfactory.cpp
    creds/credentialmanager.cpp
    creds/dummycredentials.cpp
    creds/abstractcredentials.cpp
    creds/credentialscommon.cpp
    creds/oauth.cpp

    networkjobs/checkserverjobfactory.cpp
    networkjobs/jsonjob.cpp

    abstractcorejob.cpp
    determineauthtypejobfactory.cpp

    graphapi/drives.cpp
)

if(TOKEN_AUTH_ONLY)
    set (libsync_SRCS ${libsync_SRCS} creds/tokencredentials.cpp)
else()
    set(libsync_SRCS ${libsync_SRCS} creds/httpcredentials.cpp)
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
        DESTINATION ${INCLUDE_INSTALL_DIR}/${synclib_NAME}/mirall
    )
    INSTALL(
        FILES ${creds_HEADERS}
        DESTINATION ${INCLUDE_INSTALL_DIR}/${synclib_NAME}/creds
    )
ENDIF(NOT APPLE)

add_library(libsync SHARED ${libsync_SRCS})

target_link_libraries(libsync PUBLIC
    csync
    Qt5::Core Qt5::Network
)
target_compile_definitions(libsync
    PRIVATE QT_NO_CAST_TO_ASCII
            QT_NO_CAST_FROM_ASCII
            QT_NO_URL_CAST_FROM_STRING
            QT_NO_CAST_FROM_BYTEARRAY
            QT_USE_QSTRINGBUILDER
            QT_MESSAGELOGCONTEXT  # enable function name and line number in debug output
            QT_DEPRECATED_WARNINGS
            QT_NO_FOREACH
)

target_link_libraries(libsync PUBLIC OpenAPI::LibreGraphAPI)
add_subdirectory(graphapi)

if ( APPLE )
    target_link_libraries(libsync PUBLIC
         /System/Library/Frameworks/CoreServices.framework
         /System/Library/Frameworks/Foundation.framework
         /System/Library/Frameworks/AppKit.framework
    )
endif()

if (NOT TOKEN_AUTH_ONLY)
    find_package(Qt5 REQUIRED COMPONENTS Widgets)
    target_link_libraries(libsync PUBLIC Qt5::Widgets qt5keychain)
endif()

if(Inotify_FOUND)
    target_include_directories(libsync PRIVATE ${Inotify_INCLUDE_DIRS})
    target_link_libraries(libsync PUBLIC ${Inotify_LIBRARIES})
endif()

if(NO_MSG_HANDLER)
    target_compile_definitions(libsync PRIVATE -DNO_MSG_HANDLER=1)
endif()

GENERATE_EXPORT_HEADER(libsync
        EXPORT_MACRO_NAME OWNCLOUDSYNC_EXPORT
        EXPORT_FILE_NAME owncloudlib.h
        STATIC_DEFINE OWNCLOUD_BUILT_AS_STATIC
)

set_target_properties(libsync PROPERTIES
        OUTPUT_NAME "${synclib_NAME}"
        VERSION ${MIRALL_VERSION}
        SOVERSION ${MIRALL_SOVERSION}
)
install(TARGETS libsync ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

