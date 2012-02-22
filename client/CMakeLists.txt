project(client C)

set(CLIENT_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
  CACHE INTERNAL "csync client public include directories"
)

set(CLIENT_PRIVATE_INCLUDE_DIRS
  ${CMAKE_BINARY_DIR}
  ${CSYNC_PUBLIC_INCLUDE_DIRS}
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
  ${ARGP_INCLUDE_DIRS}
)

set(CLIENT_EXECUTABLE
  csync_client
  CACHE INTERNAL "csync client"
)

set(CLIENT_LINK_LIBRARIES
  ${CLIENT_EXECUTABLE}
  ${CSYNC_LIBRARY}
)

if(NOT LINUX)
  list(APPEND CLIENT_LINK_LIBRARIES ${ARGP_LIBRARIES})
endif()

set(client_SRCS
  csync_client.c
  csync_auth.c
)

include_directories(
  ${CLIENT_PUBLIC_INCLUDE_DIRS}
  ${CLIENT_PRIVATE_INCLUDE_DIRS}
  ${CSYNC_PUBLIC_INCLUDE_DIRS}
)

add_executable(${CLIENT_EXECUTABLE} ${client_SRCS})

target_link_libraries(${CLIENT_LINK_LIBRARIES})

set_target_properties(
  ${CLIENT_EXECUTABLE}
  PROPERTIES
    OUTPUT_NAME
      csync
)

install(
  TARGETS
    csync_client
  DESTINATION
  ${BIN_INSTALL_DIR}
)
