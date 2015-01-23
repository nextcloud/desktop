project(cstdlib C)

set(CSTDLIB_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  CACHE INTERNAL "cstdlib public include directories"
)

set(CSTDLIB_PRIVATE_INCLUDE_DIRS
  ${ICONV_INCLUDE_DIR}
)

set(CSTDLIB_LIBRARY
  cstdlib
  CACHE INTERNAL "cstdlib library"
)

set(CSTDLIB_LINK_LIBRARIES
  ${CSTDLIB_LIBRARY}
)

set(cstdlib_SRCS
  c_alloc.c
  c_path.c
  c_rbtree.c
  c_string.c
  c_time.c
)

include_directories(
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
  ${CSTDLIB_PRIVATE_INCLUDE_DIRS}
)

add_library(${CSTDLIB_LIBRARY} STATIC ${cstdlib_SRCS})
if(NOT WIN32)
    add_definitions( -fPIC )
endif()
if(NOT HAVE_FNMATCH AND WIN32)
  # needed for PathMatchSpec for our fnmatch replacement
  target_link_libraries(${CSTDLIB_LIBRARY} ${SHLWAPI_LIBRARY})
endif()
