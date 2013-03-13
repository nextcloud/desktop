project(httpbf C)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked

set(HTTPBF_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  CACHE INTERNAL "httpbf public include directories"
)

set(HTTPBFLIB_PRIVATE_INCLUDE_DIRS
)

set(HBF_LIBRARY
  httpbf
  CACHE INTERNAL "httpbflib library"
)

set(HTTPBFLIB_LINK_LIBRARIES
  ${HBF_LIBRARY}
)

set(httpbflib_SRCS
  httpbf.c
)

include_directories(
  ${NEON_INCLUDE_DIRS}
  ${GLIB2_INCLUDE_DIRS}
)

if(NOT WIN32)
    add_definitions( -fPIC )
endif()

add_library(${HBF_LIBRARY} SHARED ${httpbflib_SRCS} )

target_link_libraries(${HBF_LIBRARY} ${NEON_LIBRARIES})

