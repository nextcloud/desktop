project( ocmod C )

add_definitions(-DUNIT_TESTING=1)

find_package(CMocka REQUIRED)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMOCKA_INCLUDE_DIRS}
    ${NEON_INCLUDE_DIRS}
)


add_executable(ocmod_test ocmod_test.c)
add_executable(md5_test   md5_test.c)

target_link_libraries(ocmod_test ${CMOCKA_LIBRARIES} ${NEON_LIBRARIES} ${CSYNC_LIBRARY} )

target_link_libraries(md5_test ${CMOCKA_LIBRARIES} ${CSYNC_LIBRARY} )
