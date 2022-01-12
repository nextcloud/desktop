include(ECMMarkAsTest)

# create test library
add_library(torture STATIC torture.c cmdline.c)
target_link_libraries(torture ${CMOCKA_LIBRARIES})
set_target_properties(torture PROPERTIES C_STANDARD 99 AUTOMOC OFF)
target_include_directories(torture
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
            ${PROJECT_BINARY_DIR}
            ${PROJECT_BINARY_DIR}/src
            ${CMOCKA_INCLUDE_DIR}
            ${CHECK_INCLUDE_DIRS}
)

set(TEST_TARGET_LIBRARIES torture Qt5::Core csync)

# create tests

# std

add_cmocka_test(check_std_c_jhash std_tests/check_std_c_jhash.c ${TEST_TARGET_LIBRARIES})

# vio
add_cmocka_test(check_vio_ext vio_tests/check_vio_ext.cpp ${TEST_TARGET_LIBRARIES})

