# Just list files to build as part of the csync dynamic lib.
# Essentially they could be in the same directory but are separate to
# help keep track of the different code licenses.
set(common_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/checksums.cpp
    ${CMAKE_CURRENT_LIST_DIR}/filesystembase.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ownsql.cpp
    ${CMAKE_CURRENT_LIST_DIR}/syncjournaldb.cpp
    ${CMAKE_CURRENT_LIST_DIR}/syncjournalfilerecord.cpp
    ${CMAKE_CURRENT_LIST_DIR}/utility.cpp
    ${CMAKE_CURRENT_LIST_DIR}/remotepermissions.cpp
    ${CMAKE_CURRENT_LIST_DIR}/vfs.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pinstate.cpp
    ${CMAKE_CURRENT_LIST_DIR}/syncfilestatus.cpp
)

