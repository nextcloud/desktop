project(httpbflib C)

find_package(Neon)


set(HTTPBF_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${NEON_INCLUDE_DIRS}
  CACHE INTERNAL "httpbflib public include directories"
)

set(HTTPBF_LIBRARY
  httpbf
  CACHE INTERNAL "httpbf library"
)

set(HTTPBF_LINK_LIBRARIES
  ${HTTPBF_LIBRARY}
)

set(httpbf_SRCS
  src/httpbf.c
)
set(httpbf_HEADERS
  src/httpbf.h
)

include_directories(
  ${HTTPBF_PUBLIC_INCLUDE_DIRS}
)

add_library(${HTTPBF_LIBRARY} STATIC ${httpbf_SRCS})
target_link_libraries(${HTTPBF_LIBRARY} ${NEON_LIBRARIES})

if(NOT WIN32)
    add_definitions( -fPIC )
endif()

INSTALL(
    TARGETS
    ${HTTPBF_LIBRARY}
    LIBRARY DESTINATION
    ${LIB_INSTALL_DIR}
    ARCHIVE DESTINATION
    ${LIB_INSTALL_DIR}
    RUNTIME DESTINATION
    ${BIN_INSTALL_DIR}
)

if (NOT APPLE)
    INSTALL(
        FILES
        ${httpbf_HEADERS}
        DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif (NOT APPLE)
