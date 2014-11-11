project(cmd)
set(CMAKE_AUTOMOC TRUE)

set(cmd_NAME ${APPLICATION_EXECUTABLE}cmd)
set(cmd_SRC
    cmd.cpp
    simplesslerrorhandler.cpp
    netrcparser.cpp
   )
include_directories(${CMAKE_SOURCE_DIR}/src/libsync
                    ${CMAKE_BINARY_DIR}/src/libsync
                   )

# csync is required.
include_directories(${CMAKE_SOURCE_DIR}/csync/src
                    ${CMAKE_SOURCE_DIR}/csync/src/httpbf/src
                    ${CMAKE_BINARY_DIR}/csync
                    ${CMAKE_BINARY_DIR}/csync/src
                   )
include_directories(${NEON_INCLUDE_DIRS})

if(NOT BUILD_LIBRARIES_ONLY)
   add_executable(${cmd_NAME}  ${cmd_SRC})
	qt5_use_modules(${cmd_NAME} Network Sql)
	set_target_properties(${cmd_NAME} PROPERTIES
	        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY} )
        set_target_properties(${cmd_NAME} PROPERTIES
		INSTALL_RPATH "${LIB_INSTALL_DIR}/${APPLICATION_EXECUTABLE}" )

	target_link_libraries(${cmd_NAME} ${synclib_NAME})
endif()

if(BUILD_OWNCLOUD_OSX_BUNDLE)
    install(TARGETS ${cmd_NAME} DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/MacOS)
elseif(NOT BUILD_LIBRARIES_ONLY)
    install(TARGETS ${cmd_NAME}
	    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

