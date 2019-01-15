project(cmd)
set(CMAKE_AUTOMOC TRUE)

set(cmd_NAME ${APPLICATION_EXECUTABLE}cmd)
set(cmd_SRC
    cmd.cpp
    simplesslerrorhandler.cpp
    netrcparser.cpp
   )


if(UNIX AND NOT APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
endif()

if(NOT BUILD_LIBRARIES_ONLY)
    add_executable(${cmd_NAME}  ${cmd_SRC})
    set_target_properties(${cmd_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY} )

    target_link_libraries(${cmd_NAME} "${csync_NAME}" "${synclib_NAME}" Qt5::Core Qt5::Network)

    # Need tokenizer for netrc parser
    target_include_directories(${cmd_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/3rdparty/qtokenizer)
endif()

if(BUILD_OWNCLOUD_OSX_BUNDLE)
    install(TARGETS ${cmd_NAME} DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/MacOS)
elseif(NOT BUILD_LIBRARIES_ONLY)
    install(TARGETS ${cmd_NAME}
	    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
