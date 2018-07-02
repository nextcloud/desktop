if(SPHINX_FOUND)

    # Sphinx cache with pickled ReST documents
    set(SPHINX_CACHE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_doctrees")

    # HTML output directory
    set(SPHINX_MAN_DIR      "${CMAKE_CURRENT_BINARY_DIR}/man1")
    install(DIRECTORY ${SPHINX_MAN_DIR} DESTINATION ${CMAKE_INSTALL_MANDIR} OPTIONAL)
    add_custom_target( doc-man  ${SPHINX_EXECUTABLE}
       -c ${CMAKE_BINARY_DIR}/doc -b man
       -d ${SPHINX_CACHE_DIR}/man
       ${CMAKE_CURRENT_SOURCE_DIR}
       ${SPHINX_MAN_DIR} )
endif(SPHINX_FOUND)
