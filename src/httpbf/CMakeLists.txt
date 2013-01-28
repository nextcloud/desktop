project(httpbflib C)

set(HTTPBF_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  CACHE INTERNAL "httpbflib public include directories"
)

set(HTTPBF_LIBRARY
  httpbflib
  CACHE INTERNAL "httpbf library"
)

set(HTTPBF_LINK_LIBRARIES
  ${HTTPBF_LIBRARY}
)

set(httpbflib_SRCS
  src/httpbf.c
)

include_directories(
  ${HTTPBF_PUBLIC_INCLUDE_DIRS}
)

add_library(${HTTPBF_LIBRARY} STATIC ${httpbflib_SRCS})
target_link_libraries(${HTTPBF_LIBRARY} ${NEON_LIBRARIES})

if(NOT WIN32)
    add_definitions( -fPIC )
endif()
