configure_file(fixbranding.sh.in ${CMAKE_CURRENT_BINARY_DIR}/fixbranding.sh)

install(FILES ${APPLICATION_EXECUTABLE}.py  DESTINATION ${DATADIR}/nautilus-python/extensions)
