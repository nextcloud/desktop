project(iniparser)

find_package(Iniparser)

if (INIPARSER_FOUND)
  set(INIPARSER_INCLUDE_DIRS
    ${INIPARSER_INCLUDE_DIRS}
    CACHE INTERNAL "iniparser include directory"
  )

  set(INIPARSER_LIBRARY
    ${INIPARSER_LIBRARY}
    CACHE INTERNAL "iniparser shared library"
  )

  set(INIPARSER_LIBRARIES
    ${INIPARSER_LIBRARY}
    CACHE INTERNAL "iniparser shared libraries"
  )
else (INIPARSER_FOUND)
  set(INIPARSER_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    CACHE INTERNAL "iniparser include directory"
  )

  set(INIPARSER_LIBRARY
    iniparser
    CACHE INTERNAL "iniparser static library"
  )

  set(INIPARSER_LIBRARIES
    ${INIPARSER_LIBRARY}
    CACHE INTERNAL "iniparser static libraries"
  )

  set(iniparser_SRCS
    dictionary.c
    iniparser.c
    strlib.c
  )

  add_library(${INIPARSER_LIBRARY} STATIC ${iniparser_SRCS})
endif (INIPARSER_FOUND)

