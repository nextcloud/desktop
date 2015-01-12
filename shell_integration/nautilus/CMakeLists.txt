# copy the python nautilus plugin to build dir as it is going to
# be changed.
if( UNIX AND NOT APPLE )

    configure_file(syncstate.py syncstate.py COPYONLY)

    # Call the setupappname.sh script to set the custom app name.
    set (cmd "${CMAKE_CURRENT_SOURCE_DIR}/setappname.sh")
    execute_process(COMMAND ${cmd} ${APPLICATION_SHORTNAME}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    ERROR_VARIABLE errors OUTPUT_VARIABLE out)

    install(FILES syncstate.py  DESTINATION ${DATADIR}/nautilus-python/extensions)

endif()

