project(cstdlib)

set(CSTDLIB_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  CACHE INTERNAL "cstdlib public include directories"
)

set(CSYNC_PRIVATE_INCLUDE_DIRS
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
  c_dir.c
  c_file.c
  c_path.c
  c_rbtree.c
  c_string.c
)

include_directories(
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
  ${CSTDLIB_PRIVATE_INCLUDE_DIRS}
)

add_library(${CSTDLIB_LIBRARY} STATIC ${cstdlib_SRCS})
#target_link_libraries(${CSTDLIB_LINK_LIBRARIES})
