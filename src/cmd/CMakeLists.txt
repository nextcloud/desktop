project(cmd)
set(CMAKE_AUTOMOC TRUE)

set(cmd_NAME ${APPLICATION_EXECUTABLE}cmd)

add_library(cmdCore STATIC simplesslerrorhandler.cpp netrcparser.cpp)

target_link_libraries(cmdCore
  PUBLIC
  "${csync_NAME}"
  "${synclib_NAME}"
  Qt5::Core
  Qt5::Network
  )

# Need tokenizer for netrc parser
target_include_directories(cmdCore PRIVATE ${CMAKE_SOURCE_DIR}/src/3rdparty/qtokenizer)

if(UNIX AND NOT APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
endif()

if(NOT BUILD_LIBRARIES_ONLY)
    add_executable(${cmd_NAME} cmd.cpp)
    set_target_properties(${cmd_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY} )

    target_link_libraries(${cmd_NAME} cmdCore)
endif()

# OSX: Copy nextcloudcmd to app bundle, src/gui will run macdeployqt
if(BUILD_OWNCLOUD_OSX_BUNDLE)
    add_custom_command(TARGET ${cmd_NAME} POST_BUILD
        COMMAND "cp"
            "$<TARGET_FILE:${cmd_NAME}>"
            "${BIN_OUTPUT_DIRECTORY}/${OWNCLOUD_OSX_BUNDLE}/Contents/MacOS"
        COMMENT "Copying nextcloudcmd..."
    )
elseif(NOT BUILD_LIBRARIES_ONLY)
    install(TARGETS ${cmd_NAME}
	    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
