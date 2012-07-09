project( ocmod C )

include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMOCKA_INCLUDE_DIRS}
    ${NEON_INCLUDE_DIRS}
)
find_package(CMocka REQUIRED)

add_definitions(-DUNIT_TESTING=1)

add_executable(ocmod_test ocmod_test.c)
target_link_libraries(ocmod_test ${CMOCKA_LIBRARIES} ${NEON_LIBRARIES}  ${CSYNC_LIBRARY} )

