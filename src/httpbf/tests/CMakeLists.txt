project(hbf_test  C )

add_definitions(-DUNIT_TESTING=1)

find_package(CMocka REQUIRED)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMOCKA_INCLUDE_DIRS}
    ${NEON_INCLUDE_DIRS}
    ${HTTPBF_PUBLIC_INCLUDE_DIRS}
)

add_executable(send_test hbf_send_test.c)
target_link_libraries(send_test ${CMOCKA_LIBRARIES} ${NEON_LIBRARIES} ${HBF_LIBRARY} )


