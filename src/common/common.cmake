# Just list files to build as part of the csync dynamic lib.
# Essentially they could be in the same directory but are separate to
# help keep track of the different code licenses.
configure_file(${CMAKE_CURRENT_LIST_DIR}/version.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/version.cpp @ONLY)
set(common_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/checksums.cpp
    ${CMAKE_CURRENT_LIST_DIR}/checksumalgorithms.cpp
    ${CMAKE_CURRENT_LIST_DIR}/chronoelapsedtimer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/filesystembase.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ownsql.cpp
    ${CMAKE_CURRENT_LIST_DIR}/preparedsqlquerymanager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/syncjournaldb.cpp
    ${CMAKE_CURRENT_LIST_DIR}/syncjournalfilerecord.cpp
    ${CMAKE_CURRENT_LIST_DIR}/utility.cpp
    ${CMAKE_CURRENT_LIST_DIR}/remotepermissions.cpp
    ${CMAKE_CURRENT_LIST_DIR}/vfs.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pinstate.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin.cpp
    ${CMAKE_CURRENT_LIST_DIR}/syncfilestatus.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
)

set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/plugin.cpp PROPERTIES COMPILE_DEFINITIONS APPLICATION_EXECUTABLE="${APPLICATION_EXECUTABLE}")


if(WIN32)
    list(APPEND common_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/utility_win.cpp
    )
elseif(APPLE)
    list(APPEND common_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/utility_mac.mm
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND common_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/utility_unix.cpp
    )
endif()

configure_file(${CMAKE_CURRENT_LIST_DIR}/vfspluginmetadata.json.in ${CMAKE_CURRENT_BINARY_DIR}/vfspluginmetadata.json)
