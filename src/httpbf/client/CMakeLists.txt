project(client C)

set(CLIENT_EXECUTABLE httpbfclient CACHE INTERNAL "httpbf client")

set(CLIENT_LINK_LIBRARIES ${NEON_LIBRARIES} ${HBF_LIBRARY})

set(HTTPBF_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src})

if(NOT LINUX)
  list(APPEND CLIENT_LINK_LIBRARIES ${ARGP_LIBRARIES})
endif()

set(client_SRCS
  httpbf_client.c
)

include_directories(
  ${HTTPBF_INCLUDE_DIR}
  ${HTTPBF_PUBLIC_INCLUDE_DIRS}
)

add_executable(${CLIENT_EXECUTABLE} ${client_SRCS})

target_link_libraries(${CLIENT_EXECUTABLE} ${CLIENT_LINK_LIBRARIES})

set_target_properties(
  ${CLIENT_EXECUTABLE}
  PROPERTIES
    OUTPUT_NAME
     httpbf 
)

# install( TARGETS ${CLIENT_EXECUTABLE}  DESTINATION ${BIN_INSTALL_DIR} )
install(TARGETS ${CLIENT_EXECUTABLE} DESTINATION bin) 


