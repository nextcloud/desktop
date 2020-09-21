set(TARGET_NAME NavRemove${BITNESS})

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}/../
)

add_executable(${TARGET_NAME} WIN32
    main.cpp
    ../ConfigIni.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../version.rc
)

target_link_libraries(${TARGET_NAME}
    NCToolsShared
)

install(TARGETS ${TARGET_NAME}
    DESTINATION tools/NCNavRemove/exe/
)
