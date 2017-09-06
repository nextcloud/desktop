project(cstdlib)

set(CSTDLIB_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  CACHE INTERNAL "cstdlib public include directories"
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
  c_utf8.cpp
)

if(NOT HAVE_ASPRINTF AND NOT HAVE___MINGW_ASPRINTF)
    list(APPEND cstdlib_SRCS
        asprintf.c
    )
endif()

include_directories(
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
)

add_library(${CSTDLIB_LIBRARY} STATIC ${cstdlib_SRCS})
if(NOT WIN32)
    add_definitions( -fPIC )
endif()
qt5_use_modules(${CSTDLIB_LIBRARY} Core)
