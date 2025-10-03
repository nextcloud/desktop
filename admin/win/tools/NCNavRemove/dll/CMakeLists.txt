add_definitions(-D_NAVREMOVE_EXPORTS)
add_definitions(-D_USRDLL)
add_definitions(-D_WINDLL)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}/../
)

set(TARGET_NAME libNavRemove${BITNESS})

add_library(${TARGET_NAME} MODULE
    dllmain.cpp
    NavRemove.cpp
    exports.def
    ../ConfigIni.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../version.rc
)

target_link_libraries(${TARGET_NAME}
    NCToolsShared
)

install(TARGETS ${TARGET_NAME}
    DESTINATION tools/NCNavRemove/dll/
)
