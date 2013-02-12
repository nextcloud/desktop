if(SPHINX_FOUND)

    # Sphinx cache with pickled ReST documents
    set(SPHINX_CACHE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_doctrees")
    # HTML output directory
    set(SPHINX_HTML_DIR     "${CMAKE_CURRENT_BINARY_DIR}/html")
    set(SPHINX_MAN_DIR      "${CMAKE_CURRENT_BINARY_DIR}/man")
    set(SPHINX_PDF_DIR      "${CMAKE_CURRENT_BINARY_DIR}/latex")
    set(SPHINX_QCH_DIR      "${CMAKE_CURRENT_BINARY_DIR}/qthelp")
    set(SPHINX_HTMLHELP_DIR "${CMAKE_CURRENT_BINARY_DIR}/htmlhelp")
    set(MSHTML_COMPILER     wine 'C:\\Program Files\\HTML Help Workshop\\hhc.exe')
    # assets
    set(LATEX_LOGO          "${CMAKE_CURRENT_SOURCE_DIR}/logo-blue.pdf")

    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/conf.py.in" conf.py @ONLY)
    add_custom_target(doc DEPENDS doc-html doc-man COMMENT "Building documentation...")
    if(PDFLATEX_FOUND)
        # if this still fails on Debian/Ubuntu, run
        # apt-get install texlive-latex-recommended texlive-latex-extra texlive-fonts-recommended
        add_custom_target(doc-latex  ${SPHINX_EXECUTABLE}
		                     -q -c . -b latex
				     -d ${SPHINX_CACHE_DIR}
				     -D latex_logo=${LATEX_LOGO}
				     ${CMAKE_CURRENT_SOURCE_DIR}
				     ${SPHINX_PDF_DIR} )
        add_custom_target(doc-pdf make -C ${SPHINX_PDF_DIR} all-pdf
		                  DEPENDS doc-latex )
        add_dependencies(doc doc-pdf)
    endif(PDFLATEX_FOUND)
    if (EXISTS ${QT_QCOLLECTIONGENERATOR_EXECUTABLE})
        add_custom_target( doc-qch-sphinx ${SPHINX_EXECUTABLE}
		                          -q -c . -b qthelp
				          -d ${SPHINX_CACHE_DIR}
					  ${CMAKE_CURRENT_SOURCE_DIR}
					  ${SPHINX_QCH_DIR} )
        add_custom_target( doc-qch ${QT_QCOLLECTIONGENERATOR_EXECUTABLE}
		                   ${SPHINX_QCH_DIR}/*.qhcp
				   DEPENDS doc-qch-sphinx )
        add_dependencies(doc doc-qch)
    endif()
    add_custom_target( doc-html ${SPHINX_EXECUTABLE}
	                        -q -c . -b html
				-d ${SPHINX_CACHE_DIR}
				${CMAKE_CURRENT_SOURCE_DIR}
				${SPHINX_HTML_DIR} )
    add_custom_target( doc-man  ${SPHINX_EXECUTABLE}
	                        -q -c . -b man
				-d ${SPHINX_CACHE_DIR}
				${CMAKE_CURRENT_SOURCE_DIR}
				${SPHINX_MAN_DIR} )

    ## Building CHM files requires HTML Help Workshop. Since it requires wine
    ## with special dependencies, it's impossible to write a cmake check for it.
    ## This is why doc-chm is not a dependency for doc. Instead, run
    ## doc/scripts/htmlhelp.exe to install them and run this target
    ## explicitly.
    add_custom_target( doc-chm-sphinx  ${SPHINX_EXECUTABLE}
	                               -q -c . -b htmlhelp
	                               -D html_theme=basic
                                       -d ${SPHINX_CACHE_DIR}
                                       ${CMAKE_CURRENT_SOURCE_DIR}
                                       ${SPHINX_HTMLHELP_DIR} )
    add_custom_target( doc-chm pushd ${SPHINX_HTMLHELP_DIR}; ${MSHTML_COMPILER} *.hhp; popd
				   DEPENDS doc-chm-sphinx )
endif(SPHINX_FOUND)
