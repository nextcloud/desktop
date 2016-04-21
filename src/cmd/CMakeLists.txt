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
                    ${CMAKE_BINARY_DIR}/csync
                    ${CMAKE_BINARY_DIR}/csync/src
                   )

# Need tokenizer for netrc parser
include_directories(${CMAKE_SOURCE_DIR}/src/3rdparty/qtokenizer)

if(NOT BUILD_LIBRARIES_ONLY)
   add_executable(${cmd_NAME}  ${cmd_SRC})
	qt5_use_modules(${cmd_NAME} Network Sql)
	set_target_properties(${cmd_NAME} PROPERTIES
	        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY} )
        set_target_properties(${cmd_NAME} PROPERTIES
		INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}/${APPLICATION_EXECUTABLE}" )

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


# FIXME: The following lines are dup in src/gui and src/cmd because it needs to be done after both are installed
#FIXME: find a nice solution to make the second if(BUILD_OWNCLOUD_OSX_BUNDLE) unnecessary
# currently it needs to be done because the code right above needs to be executed no matter
# if building a bundle or not and the install_qt4_executable needs to be called afterwards
if(BUILD_OWNCLOUD_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY)
   get_target_property (QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
   install(CODE "
    message(STATUS \"Deploying (Qt) dependencies and fixing library paths...\")
    execute_process(COMMAND \"${CMAKE_SOURCE_DIR}/admin/osx/macdeployqt.py\" ${CMAKE_INSTALL_PREFIX}/${OWNCLOUD_OSX_BUNDLE} ${QT_QMAKE_EXECUTABLE})
    " COMPONENT RUNTIME)
endif()
