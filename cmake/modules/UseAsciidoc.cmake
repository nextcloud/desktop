#  - macro_asciidoc2man(inputfile outputfile)
#
#  Create a manpage with asciidoc.
#  Example: macro_asciidoc2man(foo.txt foo.1)
#
# Copyright (c) 2006, Andreas Schneider, <asn@cryptomilk.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

include(MacroCopyFile)

macro(MACRO_ASCIIDOC2MAN _a2m_input _a2m_output)
  find_program(A2X
    NAMES
      a2x
  )
  #message("+++ A2X: ${A2X}")

  if (A2X)

    #message("+++ ${A2X} --doctype=manpage --format=manpage --destination-dir=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${_a2m_input}")
    macro_copy_file(${CMAKE_CURRENT_SOURCE_DIR}/${_a2m_input} ${CMAKE_CURRENT_BINARY_DIR}/${_a2m_input})

    execute_process(
      COMMAND
        ${A2X} --doctype=manpage --format=manpage ${_a2m_input}
      WORKING_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}
      RESULT_VARIABLE
        A2M_MAN_GENERATED
      ERROR_QUIET
    )

    #message("+++ A2M_MAN_GENERATED: ${A2M_MAN_GENERATED}")
    if (A2M_MAN_GENERATED EQUAL 0)
      find_file(A2M_MAN_FILE
        NAME
          ${_a2m_output}
        PATHS
          ${CMAKE_CURRENT_BINARY_DIR}
        NO_DEFAULT_PATH
      )

      if (A2M_MAN_FILE)
        get_filename_component(A2M_MAN_CATEGORY ${A2M_MAN_FILE} EXT)
        string(SUBSTRING ${A2M_MAN_CATEGORY} 1 1 A2M_MAN_CATEGORY)
        install(
          FILES
            ${A2M_MAN_FILE}
          DESTINATION
            ${MAN_INSTALL_DIR}/man${A2M_MAN_CATEGORY}
        )
      endif (A2M_MAN_FILE)
    endif (A2M_MAN_GENERATED EQUAL 0)

  endif (A2X)
endmacro(MACRO_ASCIIDOC2MAN _a2m_input _a2m_file)
