project( ocmod C )

add_definitions(-DUNIT_TESTING=1)

find_package(CMocka REQUIRED)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMOCKA_INCLUDE_DIRS}
    ${NEON_INCLUDE_DIRS}
    ${HTTPBF_PUBLIC_INCLUDE_DIRS}
)


add_executable(ocmod_test ocmod_test.c  ${CMAKE_SOURCE_DIR}/modules/csync_owncloud_recursive_propfind.c
	${CMAKE_SOURCE_DIR}/modules/csync_owncloud_util.c)

target_link_libraries(ocmod_test ${CMOCKA_LIBRARIES} ${NEON_LIBRARIES} ${CSYNC_LIBRARY} ${HTTPBF_LIBRARY})

